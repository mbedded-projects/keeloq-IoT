import 'dart:async';
import 'dart:io';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:connectivity_plus/connectivity_plus.dart';
import 'package:http/http.dart' as http;
import 'package:mqtt_client/mqtt_client.dart';
import 'package:mqtt_client/mqtt_server_client.dart';

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setPreferredOrientations([
    DeviceOrientation.portraitUp,
  ]);
  runApp(const GateControlApp());
}

/// Główna aplikacja do sterowania bramą
class GateControlApp extends StatelessWidget {
  const GateControlApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Sterowanie Bramą',
      theme: ThemeData.light().copyWith(
        scaffoldBackgroundColor: Colors.white,
      ),
      home: const GateControlPage(),
    );
  }
}

/// Strona główna z kontrolką bramy
class GateControlPage extends StatefulWidget {
  const GateControlPage({super.key});

  @override
  State<GateControlPage> createState() => _GateControlPageState();
}

/// Enum reprezentujący różne stany bramy
enum GateState {
  closed,     // Bram zamknięta
  closing,    // Brama w trakcie zamykania
  open,       // Brama otwarta
  opening,    // Brama w trakcie otwierania
  partiallyOpen, // Brama częściowo otwarta
  unknown,    // Stan nieznany
}

class _GateControlPageState extends State<GateControlPage> {
  GateState _gateState = GateState.unknown;
  static const double visiblePart = 50.0;

  // Dane uwierzytelniające
  final String _username = "";
  final String _apiKey = "";

  // Połączenie MQTT
  late MqttServerClient _mqttClient;
  bool _mqttConnected = false;
  String _connectionStatus = 'Oczekiwanie na połączenie...';
  final String _feedName = "gateway";

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      _initConnectivityMonitoring();
      await _initializeMqttConnection();
    });
  }


  /// Inicjalizuje monitorowanie połączenia sieciowego
  Future<void> _initConnectivityMonitoring() async {
    final connectivity = await Connectivity().checkConnectivity();
    if (connectivity == ConnectivityResult.none) {
      _updateConnectionState(
        connected: false,
        status: 'Brak połączenia internetowego',
        gateState: GateState.unknown,
      );
      _showNoInternetDialog();
    }
    
    Connectivity().onConnectivityChanged.listen((status) {
      if (status == ConnectivityResult.none) {
        _updateConnectionState(
          connected: false,
          status: 'Brak połączenia internetowego',
          gateState: GateState.unknown,
        );
        _showNoInternetDialog();
      }
    });
  }

  /// Inicjalizuje połączenie MQTT
  Future<void> _initializeMqttConnection() async {    
    _updateConnectionState(
      status: 'Łączenie...',
      gateState: GateState.unknown,
    );
    
    final broker = 'io.adafruit.com';
    final clientId = '${DateTime.now().millisecondsSinceEpoch}';
    
    _mqttClient = MqttServerClient.withPort(broker, clientId, 8883)
      ..secure = true
      ..logging(on: false)
      ..keepAlivePeriod = 20;
      
    _mqttClient.setProtocolV311();
    _mqttClient.securityContext = SecurityContext.defaultContext;

    _mqttClient.connectionMessage = MqttConnectMessage()
        .withClientIdentifier(clientId)
        .authenticateAs(_username, _apiKey);

    _mqttClient.onConnected = () async {
      debugPrint('Połączono z brokerem MQTT');
      _updateConnectionState(
        connected: true,
        status: 'Połączono',
      );

      // Pobierz ostatni stan bramy przez REST
      final lastValue = await _fetchLastGateState();
      _updateGateStateFromString(lastValue);

      // Subskrybuj temat MQTT
      final topic = '$_username/feeds/$_feedName';
      if(_mqttClient.getSubscriptionsStatus(topic) == MqttSubscriptionStatus.doesNotExist) {
        _mqttClient.subscribe(topic, MqttQos.exactlyOnce);
        _mqttClient.updates?.listen(_handleMqttMessage);
      }
    };

    _mqttClient.onDisconnected = () {
      debugPrint('Rozłączono z brokerem MQTT');
      _updateConnectionState(
        connected: false,
        status: 'Rozłączono',
        gateState: GateState.unknown,
      );
    };

    _mqttClient.autoReconnect = true;
    _mqttClient.resubscribeOnAutoReconnect = true;
    _mqttClient.onAutoReconnected = () {
      debugPrint('Automatyczne ponowne połączenie');
      _updateConnectionState(status: 'Połączono ponownie');
    };

    try {
      await _mqttClient.connect();
    } catch (e) {
      debugPrint('Błąd połączenia MQTT: $e');
      _updateConnectionState(
        status: 'Błąd połączenia: $e',
        gateState: GateState.unknown,
      );
      _mqttClient.disconnect();
    }
  }

  /// Aktualizuje stan połączenia i interfejs użytkownika
  void _updateConnectionState({
    bool? connected,
    String? status,
    GateState? gateState,
  }) {
    setState(() {
      if (connected != null) _mqttConnected = connected;
      if (status != null) _connectionStatus = status;
      if (gateState != null) _gateState = gateState;
    });
  }

  /// Konwertuje ciąg znaków na stan bramy
  void _updateGateStateFromString(String? state) {
    if (state == null) {
      setState(() => _gateState = GateState.unknown);
      return;
    }

    final stateLower = state.toLowerCase();
    setState(() {
      if (stateLower == 'toggle') {
        return;
      } else if (stateLower == 'closed') {
        _gateState = GateState.closed;
      } else if (stateLower == 'closing') {
        _gateState = GateState.closing;
      } else if (stateLower == 'open') {
        _gateState = GateState.open;
      } else if (stateLower == 'opening') {
        _gateState = GateState.opening;
      } else if (stateLower == 'partially_open') {
        _gateState = GateState.partiallyOpen;
      } else {
        _gateState = GateState.unknown;
      }
    });
  }

  /// Pobiera ostatni stan bramy z serwera
  Future<String?> _fetchLastGateState() async {
    try {
      final url = Uri.https(
        'io.adafruit.com',
        '/api/v2/$_username/feeds/$_feedName/data/last',
      );
      final response = await http.get(
        url,
        headers: {'X-AIO-Key': _apiKey},
      );
      
      if (response.statusCode == 200) {
        final jsonData = jsonDecode(response.body) as Map<String, dynamic>;
        return jsonData['value'] as String?;
      } else {
        debugPrint('Błąd REST API: ${response.statusCode}');
        return null;
      }
    } catch (e) {
      debugPrint('Błąd pobierania stanu: $e');
      return null;
    }
  }

  /// Obsługuje przychodzące wiadomości MQTT
  void _handleMqttMessage(List<MqttReceivedMessage<MqttMessage>> messages) {
    for (final message in messages) {
      final payload = message.payload as MqttPublishMessage;
      final content = MqttPublishPayload.bytesToStringAsString(payload.payload.message);
      debugPrint('Odebrano wiadomość: $content');
      
      if (message.topic == '$_username/feeds/$_feedName') {
        _updateGateStateFromString(content);
      }
    }
  }

  /// Wysyła komendę zmiany stanu bramy
  void _sendGateCommand() {
    if (!_mqttConnected) {
      _showNoInternetDialog();
      return;
    }
    
    final builder = MqttClientPayloadBuilder()..addString('toggle');
    final topic = '$_username/feeds/$_feedName';
    
    _mqttClient.publishMessage(topic, MqttQos.atLeastOnce, builder.payload!);
    debugPrint('Wysłano komendę: toggle');
  }

  /// Wyświetla dialog o braku połączenia internetowego
  void _showNoInternetDialog() {
    WidgetsBinding.instance.addPostFrameCallback((_) {
      showDialog(
        context: context,
        barrierDismissible: false,
        builder: (_) => AlertDialog(
          title: const Text('Brak Internetu'),
          content: const Text('Sprawdź połączenie internetowe.'),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: const Text('OK'),
            ),
          ],
        ),
      );
    });
  }

  /// Tekst opisujący aktualny stan bramy
  String get _gateStatusText {
    switch (_gateState) {
      case GateState.closed:
        return 'Brama jest zamknięta';
      case GateState.closing:
        return 'Brama się zamyka...';
      case GateState.open:
        return 'Brama jest otwarta';
      case GateState.opening:
        return 'Brama się otwiera...';
      case GateState.partiallyOpen:
        return 'Brama częściowo otwarta';
      case GateState.unknown:
        return 'Stan nieznany';
    }
  }

  /// Pozycja bramy w animacji
  double get _gatePosition {
    final screenWidth = MediaQuery.of(context).size.width;
    
    switch (_gateState) {
      case GateState.open:
        return -(screenWidth - visiblePart);
      case GateState.opening:
        return -(screenWidth - visiblePart) / 2;
      case GateState.partiallyOpen:
        return -(screenWidth - visiblePart) / 2;
      case GateState.closing:
        return -(screenWidth - visiblePart) / 2;
      case GateState.closed:
      case GateState.unknown:
      default:
        return 0;
    }
  }

  /// Ikona przycisku w zależności od stanu bramy
  IconData get _buttonIcon {
    switch (_gateState) {
      case GateState.open:
        return Icons.lock;
      case GateState.partiallyOpen:
        return Icons.sync;
      case GateState.closed:
        return Icons.lock_open;
      case GateState.closing:
      case GateState.opening:
        return Icons.stop;
      case GateState.unknown:
        return Icons.question_mark;
    }
  }

  @override
  Widget build(BuildContext context) {
    final screenWidth = MediaQuery.of(context).size.width;
    const double gateHeight = 200.0;

    return Scaffold(
      body: SafeArea(
        child: Column(
          children: [
            const SizedBox(height: 20),
            // Status połączenia
            Center(
              child: Text(
                _connectionStatus,
                style: const TextStyle(color: Colors.blueGrey),
              ),
            ),
            const SizedBox(height: 80),
            // Animowany tekst stanu bramy
            AnimatedSwitcher(
              duration: const Duration(milliseconds: 800),
              switchOutCurve: const Interval(0.0, 0.6, curve: Curves.easeOut),
              switchInCurve: const Interval(0.4, 1.0, curve: Curves.easeIn),
              layoutBuilder: (current, previous) => Stack(
                alignment: Alignment.center,
                children: [...previous, if (current != null) current],
              ),
              transitionBuilder: (child, animation) => 
                  FadeTransition(opacity: animation, child: child),
              child: Text(
                _gateStatusText,
                key: ValueKey<GateState>(_gateState),
                textScaler: TextScaler.linear(2.3),
                style: const TextStyle(fontWeight: FontWeight.bold),
              ),
            ),
            const Spacer(),
            // Wizualizacja bramy
            SizedBox(
              width: screenWidth,
              height: gateHeight,
              child: Stack(
                children: [
                  AnimatedPositioned(
                    duration: const Duration(milliseconds: 800),
                    curve: Curves.easeInOut,
                    left: _gatePosition,
                    top: 0,
                    width: screenWidth,
                    height: gateHeight,
                    child: Image.asset('assets/gate.jpg', fit: BoxFit.cover),
                  ),
                ],
              ),
            ),
            const Spacer(),
            // Przycisk sterujący
            ElevatedButton(
              onPressed: _sendGateCommand,
              style: ElevatedButton.styleFrom(
                shape: const CircleBorder(),
                padding: const EdgeInsets.all(96),
                elevation: 8,
                backgroundColor: const Color(0xFF2C2C2C),
              ),
              child: AnimatedSwitcher(
                duration: const Duration(milliseconds: 300),
                transitionBuilder: (child, anim) => 
                    ScaleTransition(scale: anim, child: child),
                child: Icon(
                  _buttonIcon,
                  key: ValueKey<GateState>(_gateState),
                  size: 40,
                  color: Colors.white,
                ),
              ),
            ),
            const SizedBox(height: 60),
          ],
        ),
      ),
    );
  }
}