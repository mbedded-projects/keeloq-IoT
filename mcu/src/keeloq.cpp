#include "keeloq.h"

KeeLoq::KeeLoq(uint32_t cs, uint32_t irq, uint32_t rst)
    : csPin_(cs),
      irqPin_(irq),
      rstPin_(rst),
      mod_(cs, irq, rst, RADIOLIB_NC),
      radio_(&mod_) {}

KeeLoq::KeeLoq(uint32_t cs, uint32_t irq, uint32_t rst, SPIClass *spi)
    : csPin_(cs),
      irqPin_(irq),
      rstPin_(rst),
      mod_(cs, irq, rst, RADIOLIB_NC, spi ? *spi : SPI),
      radio_(&mod_) {}

int KeeLoq::begin(float freq, float br,
                  float rxBw, int8_t pwr)
{
    // Configure control pins
    pinMode(csPin_, OUTPUT);
    pinMode(irqPin_, INPUT);
    pinMode(rstPin_, OUTPUT);

    // Prepare ISR buffers
    lastMicros_ = micros();
    pulseCount_ = 0;
    attachInterruptArg(digitalPinToInterrupt(irqPin_), handleEdge, this, CHANGE);

    // Initialize CC1101 radio
    int16_t status = radio_.begin(freq, br, 0, rxBw, pwr);
    if (status != RADIOLIB_ERR_NONE)
    {
        return -1;
    }
    radio_.setOOK(true);
    radio_.setPromiscuousMode(true);
    radio_.setDIOMapping(0, 0x0D);

    // Start direct RX asynchronously
    status = radio_.receiveDirectAsync();
    return (status == RADIOLIB_ERR_NONE ? 0 : -2);
}

uint32_t KeeLoq::decrypt(const uint64_t key, const uint32_t data)
{
    uint32_t x = data;
    uint32_t keyLo = static_cast<uint32_t>(key);
    uint32_t keyHi = static_cast<uint32_t>(key >> 32);
    for (int r = 0; r < 528; ++r)
    {
        int kb = (15 - r) & 63;
        uint32_t kbv = (kb < 32)
                           ? ((keyLo >> kb) & 1)
                           : ((keyHi >> (kb - 32)) & 1);

        int idx = ((x >> 0) & 1) | (((x >> 8) & 1) << 1) | (((x >> 19) & 1) << 2) | (((x >> 25) & 1) << 3) | (((x >> 30) & 1) << 4);

        uint32_t nb = ((x >> 31) & 1) ^ ((x >> 15) & 1) ^ ((KEELOQ_NLF >> idx) & 1) ^ kbv;

        x = (x << 1) | (nb & 1);
    }
    return x;
}

RawFrame KeeLoq::parse(const uint32_t *p, int count) const
{
    RawFrame rf = {};

    // Znajdź impuls synchronizacji
    for (int i = 0; i < count; i++)
    {
        if (p[i] >= SYNC_MIN && p[i] <= SYNC_MAX)
        {
            int syncIdx = i;
            int syncGapIdx = syncIdx + 132;
            // Sprawdzenie czy we właściwym miejscu jest synchronizacja końca ramki
            if (syncGapIdx >= count || p[syncGapIdx] < GAP_MIN_US || p[syncGapIdx] > GAP_MAX_US)
                continue;

            int idx = syncIdx + 1;

            for (int k = 0; k < 66; k++)
            {
                rf.bits[k] = (p[idx + 2 * k] >= 600) ? 0 : 1;
            }
            return rf;
        }
    }
    // Jeśli nie znaleziono synchronizacji, zwróć pustą ramkę
    return {};
}

Frame KeeLoq::decodeFrame(const RawFrame &rawFrame) const
{
    Frame f = {};

    // 1. Odwróć kolejność bitów (zgodnie z implementacją producenta)
    uint8_t rev[FRAME_BITS];
    for (int i = 0; i < FRAME_BITS; i++)
    {
        rev[i] = rawFrame.bits[FRAME_BITS - 1 - i];
    }

    // 2. Dekoduj stałą część (34 bity)
    f.repeat = rev[0];
    f.v_low = rev[1];
    f.button = (rev[2] << 3) | (rev[3] << 2) | (rev[4] << 1) | rev[5];

    f.serial = 0;
    for (int i = 6; i <= 33; i++)
    {
        f.serial = (f.serial << 1) | rev[i];
    }

    // 3. Dekoduj zaszyfrowaną część (32 bity)
    f.cipher_part = 0;
    for (int i = 34; i < FRAME_BITS; i++)
    {
        f.cipher_part = (f.cipher_part << 1) | rev[i];
    }

    // 4. Zapisz całą stałą część (dla celów diagnostycznych)
    f.fixed_part = 0;
    for (int i = 0; i < 34; i++)
    {
        f.fixed_part = (f.fixed_part << 1) | rev[i];
    }

    return f;
}

void IRAM_ATTR KeeLoq::handleEdge(void *arg)
{
    auto *self = static_cast<KeeLoq *>(arg);
    uint32_t now = micros();
    if (self->pulseCount_ < MAX_PULSES)
    {
        self->pulses_[self->pulseCount_] = now - self->lastMicros_;
        ++self->pulseCount_;
    }
    self->lastMicros_ = now;
}

void KeeLoq::loop()
{
    if (!receiveCallback_)
        return;
    uint32_t gap;
    uint16_t cnt;
    noInterrupts();
    gap = micros() - lastMicros_;
    cnt = pulseCount_;
    interrupts();

    if (cnt > 0 && gap > 15000)
    {
        radio_.standby();
        detachInterrupt(digitalPinToInterrupt(irqPin_));

        uint32_t buf[MAX_PULSES];
        noInterrupts(); // noInterrupts is much quicker then detachInterrupt
        memcpy(buf, (const void *)pulses_, sizeof(uint32_t) * cnt);
        pulseCount_ = 0;
        interrupts();

        RawFrame raw = parse(buf, cnt);
        Frame frame = {0};
        frame = decodeFrame(raw);
        for (auto &rem : validRemotes_)
        {
            if (rem.serial == frame.serial)
            {
                Frame decryptedFrame = decryptFrame(frame, rem.deviceKey);
                if (decryptedFrame.valid &&
                    rem.disc == decryptedFrame.disc &&
                    rem.counter < decryptedFrame.counter + (decryptedFrame.ovr << 16))
                {
                    decryptedFrame.remote = rem;
                    decryptedFrame.remote.counter = decryptedFrame.counter;
                    receiveCallback_(decryptedFrame, raw, buf, cnt);
                }
                else
                    decryptedFrame.valid = false;
                break;
            }
        }

        if (validRemotes_.empty())
        {
            receiveCallback_(frame, raw, buf, cnt);
        }

        lastMicros_ = micros();
        attachInterruptArg(digitalPinToInterrupt(irqPin_), handleEdge, this, CHANGE);
        radio_.receiveDirectAsync();
    }
}

void KeeLoq::setFilteredRemotes(const std::vector<Remote> &remotes)
{
    validRemotes_ = remotes;
}

void KeeLoq::setReceiveCallback(ReceiveCallback cb)
{
    receiveCallback_ = cb;
}

String KeeLoq::frameToString(const Frame &f, bool printMore)
{
    String s = "valid: " + String(f.valid) + ", repeat: " + String(f.repeat) + ", v_low: " + String(f.v_low) + ", button: " + String(f.button) + ", serial: 0x" + String(f.serial, HEX) + ", ovr: " + String(f.ovr) + ", disc: " + String(f.disc) + ", counter: " + String(f.counter);
    if (printMore)
    {
        s += ", cipher: 0x" + String(f.cipher_part, HEX) + ", fixed:  0x" + String(f.fixed_part, HEX);
    }
    s += '\n';
    return s;
}

Frame KeeLoq::decryptFrame(const Frame &frame, uint64_t key)
{
    Frame decryptedFrame = frame;
    uint32_t dec = decrypt(key, frame.cipher_part);

    decryptedFrame.ovr = (dec >> 26) & 0x3;
    decryptedFrame.disc = (dec >> 16) & 0x3FF;
    decryptedFrame.counter = (dec >> 0) & 0xFFFF;

    uint8_t btn = (dec >> 28) & 0xF;
    decryptedFrame.valid = (decryptedFrame.button == btn);
    return decryptedFrame;
}
