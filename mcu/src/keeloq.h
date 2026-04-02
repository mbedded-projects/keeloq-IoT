#ifndef KEELOQ_H
#define KEELOQ_H

#include <Arduino.h>
#include <RadioLib.h>
#include <vector>
#include <cstdint>

/**
 * @brief Represents a known KeeLoq remote device.
 */
struct Remote
{
    uint32_t serial;    ///< 28‑bit unique device serial number
    uint64_t deviceKey; ///< 64‑bit encryption key
    uint32_t counter;   ///< Last seen counter value (16 bits + OVR)
    uint16_t disc;      ///< 10‑bit discrimination code
};

/**
 * @brief Decoded KeeLoq frame contents.
 */
struct Frame
{
    bool valid;           ///< True if decryption and validation succeeded
    uint8_t repeat;       ///< Repeat flag bit
    uint8_t v_low;        ///< Low‑battery flag bit
    uint8_t button;       ///< Button ID (0–15)
    uint32_t serial;      ///< 28‑bit device serial
    uint8_t ovr;          ///< 2‑bit overflow field
    uint16_t disc;        ///< 10‑bit discrimination field
    uint16_t counter;     ///< 16‑bit event counter
    uint32_t cipher_part; ///< 32‑bit encrypted payload
    uint64_t fixed_part;  ///< 34‑bit fixed header bits
    Remote remote;        ///< Matching remote record
};

/**
 * @brief Raw bitstream captured from RF pulses.
 */
struct RawFrame
{
    uint8_t bits[66]; ///< Bit values (66)
};

/**
 * @brief Driver class for receiving and decoding KeeLoq frames via CC1101 radio.
 */
class KeeLoq
{
private:
    // Protocol timing and buffer limits
    static constexpr int MAX_PULSES = 2048;  ///< Max pulses per frame
    static constexpr int GAP_MIN_US = 16000; ///< 16400us gap signals end of frame
    static constexpr int GAP_MAX_US = 16800; ///<
    static constexpr int SYNC_MIN = 3800;    ///< Minimum sync LOW duration (µs)
    static constexpr int SYNC_MAX = 4200;    ///< Maximum sync LOW duration (µs)
    static constexpr int FRAME_BITS = 66;    ///< Expected number of frame bits

    uint32_t csPin_, irqPin_, rstPin_; ///< SPI CS, interrupt, reset pins
    Module mod_;                       ///< Underlying hardware module
    CC1101 radio_;                     ///< CC1101 radio instance

    // ISR data structures
    volatile uint32_t pulses_[MAX_PULSES]; ///< Pulse capture buffer
    volatile uint16_t pulseCount_ = 0;     ///< Number of pulses captured
    volatile uint32_t lastMicros_;         ///< Timestamp of last edge

    using ReceiveCallback = void (*)(Frame &frame,
                                     const RawFrame &rawFrame,
                                     const uint32_t durations[],
                                     int n);

    std::vector<Remote> validRemotes_;          ///< Whitelisted remotes
    ReceiveCallback receiveCallback_ = nullptr; ///< User callback

    static constexpr uint32_t KEELOQ_NLF = 0x3A5C742E; ///< Non‑linear function constant

    /**
     * @brief Decrypts a 32‑bit KeeLoq word using a 64‑bit key.
     * @param key  64‑bit device key.
     * @param data 32‑bit encrypted data.
     * @return Decrypted 32‑bit output.
     */
    static uint32_t decrypt(const uint64_t key, const uint32_t data);

    /**
     * @brief Parses raw pulses into bit-level KeeLoq frame structure.
     * @param p     Pointer to pulse buffer.
     * @param count Number of pulses available.
     * @return Decoded RawFrame with up to FRAME_BITS bits.
     */
    RawFrame parse(const uint32_t *p, int count) const;

    /**
     * @brief Converts a RawFrame into a high‑level Frame structure.
     * @param rawFrame Raw bitstream to decode.
     * @return Fully populated Frame.
     */
    Frame decodeFrame(const RawFrame &rawFrame) const;

    /**
     * @brief Interrupt Service Routine for pin change events.
     * @param arg Pointer to KeeLoq instance.
     */
    static void IRAM_ATTR handleEdge(void *arg);

public:
    /**
     * @brief Constructs a KeeLoq driver.
     * @param cs   Chip select pin for CC1101.
     * @param irq  Interrupt pin connected to CC1101 data output.
     * @param rst  Reset pin for CC1101.
     */
    KeeLoq(uint32_t cs, uint32_t irq, uint32_t rst);

    /**
     * @brief Constructs a KeeLoq driver.
     * @param cs   Chip select pin for CC1101.
     * @param irq  Interrupt pin connected to CC1101 data output.
     * @param rst  Reset pin for CC1101.
     * @param spi  SPI bus instance.
     */
    KeeLoq(uint32_t cs, uint32_t irq, uint32_t rst, SPIClass *spi);

    /**
     * @brief Initializes the radio and interrupt for direct RX.
     * @param freq            Carrier frequency in MHz (default 433.92).
     * @param br              Data rate in kBd (default 2.4).
     * @param freqDev         Frequency deviation in MHz.
     * @param rxBw            Receiver bandwidth in kHz.
     * @param pwr             TX power in dBm.
     * @param preambleLength  Preamble length in bits.
     * @return 0 on success; negative error code on failure.
     */
    int begin(float freq = 433.92F,
              float br = 2.4F,
              float rxBw = 58.0F,
              int8_t pwr = 10);

    /**
     * @brief Sets the callback invoked when a frame is received.
     * @param cb User-provided function to handle decoded frames.
     */
    void setReceiveCallback(ReceiveCallback cb);

    /**
     * @brief Sets the list of allowed remotes, used for filtering and decryption.
     *
     * If the list is empty, no filtering or decryption will be applied.
     *
     * @param remotes A vector of Remote structs representing allowed remotes.
     */
    void setFilteredRemotes(const std::vector<Remote> &remotes);

    /**
     * @brief Main processing loop; must be called regularly in loop().
     */
    void loop();

    /**
     * @brief Formats a Frame as a printable String.
     * @param frame     Frame to format.
     * @param printMore If true, includes cipher_part and fixed_part.
     * @return Human‑readable description.
     */
    static String frameToString(const Frame &frame, bool printMore = false);

    /**
     * @brief Decrypts cypher part of the frame and puts values into new Frame.
     * Parameter "valid" is true when button in fixed part is the same as in cypher.
     * @param frame Frame to decrypt.
     * @param key Device key.
     * @return Decrypted frame.
     */
    static Frame decryptFrame(const Frame &frame, uint64_t key);
};

#endif // KEELOQ_H
