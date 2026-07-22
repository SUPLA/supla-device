# Topiki MQTT

Języki: [English](topics.md) · **Polski**

Ten dokument opisuje topiki MQTT udostępniane przez urządzenia SUPLA 
pracujące w trybie MQTT.

`{prefix}` oznacza `[custom-prefix/]supla/devices/{hostname}`. 
`{channel}` jest numerem kanału. 
`{phase}` jest numerem fazy licznika energii elektrycznej.

Topiki w sekcji **Subskrybowane topiki** przyjmują polecenia wysyłane do urządzenia. 
Topiki w sekcji **Publikowane topiki** zawierają stany i pomiary publikowane przez urządzenie.

Dokładny zestaw dostępnych topików zależy od konfiguracji urządzenia, 
typów i funkcji kanałów oraz obsługiwanych pomiarów.

## Typy i funkcje kanałów

[Urządzenie](#urządzenie) · [Przekaźnik](#przekaźnik) · [Roleta](#roleta) · [Ściemniacz](#ściemniacz) · [Sterownik RGB](#sterownik-rgb) · [Sterownik ściemniacza i RGB](#sterownik-ściemniacza-i-rgb) · [Termometr](#termometr) · [Czujnik wilgotności i temperatury](#czujnik-wilgotności-i-temperatury) · [HVAC](#hvac) · [Licznik energii elektrycznej](#licznik-energii-elektrycznej) · [Czujnik binarny](#czujnik-binarny)

## Urządzenie

### Publikowane topiki

#### Stan połączenia

- Topik: `{prefix}/state/connected`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `true`
- Dostępność: Wszystkie urządzenia z włączonym MQTT.

Stan dostępności urządzenia przez MQTT.

Przykład: `true`

#### Czas trwania połączenia

- Topik: `{prefix}/state/connection_uptime`
- Typ payloadu: `integer`
- Jednostka: `s`
- QoS: `0`
- Retain: `false`
- Dostępność: Urządzenia, które nie pracują w trybie uśpienia.

Czas od nawiązania bieżącego połączenia.

Przykład: `0`

#### Adres IP

- Topik: `{prefix}/state/ip`
- Typ payloadu: `string`
- QoS: `0`
- Retain: `false`
- Dostępność: Publikowany podczas rejestracji MQTT.

Adres IPv4 zgłaszany przez aktywny interfejs sieciowy.

Przykład: `192.0.2.10`

#### Adres MAC

- Topik: `{prefix}/state/mac`
- Typ payloadu: `string`
- QoS: `0`
- Retain: `false`
- Dostępność: Publikowany podczas rejestracji MQTT.

Główny adres MAC urządzenia w zapisie szesnastkowym rozdzielonym dwukropkami.

Przykład: `01:02:03:04:05:AB`

#### RSSI Wi-Fi

- Topik: `{prefix}/state/rssi`
- Typ payloadu: `integer`
- Jednostka: `dBm`
- QoS: `0`
- Retain: `false`
- Dostępność: Interfejsy Wi-Fi udostępniające RSSI.

Poziom odbieranego sygnału Wi-Fi zgłaszany przez urządzenie.

Przykład: `-67`

#### Czas pracy urządzenia

- Topik: `{prefix}/state/uptime`
- Typ payloadu: `integer`
- Jednostka: `s`
- QoS: `0`
- Retain: `false`
- Dostępność: Wszystkie urządzenia z włączonym MQTT.

Czas pracy urządzenia w sekundach.

Przykład: `0`

#### Siła sygnału Wi-Fi

- Topik: `{prefix}/state/wifi_signal_strength`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `false`
- Dostępność: Interfejsy Wi-Fi udostępniające siłę sygnału.

Znormalizowana siła sygnału Wi-Fi zgłaszana przez urządzenie.

Przykład: `73`

## Przekaźnik

Typ kanału: `SUPLA_CHANNELTYPE_RELAY`

Funkcje kanału: `SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND`, `SUPLA_CHANNELFNC_CONTROLLINGTHEGATE`, `SUPLA_CHANNELFNC_LIGHTSWITCH`, `SUPLA_CHANNELFNC_POWERSWITCH`

### Subskrybowane topiki

#### Wykonaj akcję przekaźnika

- Topik: `{prefix}/channels/{channel}/execute_action`
- Typ payloadu: `string`
- Dozwolone wartości: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały przekaźnikowe z funkcją wyłącznika zasilania.

Włącza, wyłącza lub przełącza bieżący stan przekaźnika. Wielkość liter nie ma znaczenia.

Przykład: `turn_on`

#### Ustaw stan przekaźnika

- Topik: `{prefix}/channels/{channel}/set/on`
- Typ payloadu: `string`
- Dozwolone wartości: `true`, `1`, `yes`, `false`, `0`, `no`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały z funkcją przekaźnika.

Zmienia stan wyjścia przekaźnikowego. Wielkość liter nie ma znaczenia.

Obecny parser interpretuje każdą wartość inną niż `1`, `yes` lub `true` jako OFF. Integracje powinny używać wyłącznie udokumentowanych wartości.

Przykład: `true`

### Publikowane topiki

#### Stan kalibracji

- Topik: `{prefix}/channels/{channel}/state/is_calibrating`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `true`
- Dostępność: Rolety i zgodne funkcje osłon.

Informuje, czy trwa kalibracja osłony.

Przykład: `false`

#### Stan przekaźnika impulsowego

- Topik: `{prefix}/channels/{channel}/state/on`
- Typ payloadu: `string`
- Dozwolone wartości: `closed`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały przekaźnikowe z funkcją wyłącznika zasilania lub światła.

Stan spoczynkowy zgłaszany przez przekaźnik impulsowy bramy, garażu lub zamka.

Przykłady: `closed`, `false`, `true`

#### Procent zamknięcia

- Topik: `{prefix}/channels/{channel}/state/shut`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `true`
- Dostępność: Rolety i zgodne funkcje osłon.

Bieżąca pozycja osłony, gdzie 0 oznacza pełne otwarcie, a 100 pełne zamknięcie.

Przykład: `40`

#### Pozycja nachylenia

- Topik: `{prefix}/channels/{channel}/state/tilt`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje osłon obsługujące nachylenie lameli.

Bieżąca pozycja nachylenia lameli.

Przykład: `60`

## Roleta

Typ kanału: `SUPLA_CHANNELTYPE_RELAY`

Funkcja kanału: `SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER`

### Subskrybowane topiki

#### Wykonaj akcję rolety

- Topik: `{prefix}/channels/{channel}/execute_action`
- Typ payloadu: `string`
- Dozwolone wartości: `reveal`, `shut`, `stop`, `calibrate`, `recalibrate`
- QoS: `0`
- Retain: `false`
- Dostępność: Rolety i zgodne funkcje osłon.

Otwiera, zamyka, zatrzymuje lub ponownie kalibruje roletę. Wielkość liter nie ma znaczenia.

Przykład: `reveal`

#### Ustaw procent zamknięcia

- Topik: `{prefix}/channels/{channel}/set/closing_percentage`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `false`
- Dostępność: Rolety i zgodne funkcje osłon.

Ustawia pozycję osłony, gdzie 0 oznacza pełne otwarcie, a 100 pełne zamknięcie.

Przykład: `10`

#### Ustaw nachylenie

- Topik: `{prefix}/channels/{channel}/set/tilt`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `false`
- Dostępność: Funkcje osłon obsługujące nachylenie lameli.

Ustawia procentowe nachylenie lameli.

Przykład: `20`

### Publikowane topiki

#### Stan kalibracji

- Topik: `{prefix}/channels/{channel}/state/is_calibrating`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `true`
- Dostępność: Rolety i zgodne funkcje osłon.

Informuje, czy trwa kalibracja osłony.

Przykład: `false`

#### Procent zamknięcia

- Topik: `{prefix}/channels/{channel}/state/shut`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `true`
- Dostępność: Rolety i zgodne funkcje osłon.

Bieżąca pozycja osłony, gdzie 0 oznacza pełne otwarcie, a 100 pełne zamknięcie.

Przykład: `33`

## Ściemniacz

Typ kanału: `SUPLA_CHANNELTYPE_DIMMER`

Funkcja kanału: `SUPLA_CHANNELFNC_DIMMER`

### Subskrybowane topiki

#### Wykonaj akcję ściemniacza

- Topik: `{prefix}/channels/{channel}/execute_action`
- Typ payloadu: `string`
- Dozwolone wartości: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące ściemnianie.

Włącza, wyłącza lub przełącza bieżący stan ściemniacza. Wielkość liter nie ma znaczenia.

Przykład: `turn_on`

#### Ustaw jasność

- Topik: `{prefix}/channels/{channel}/set/brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące ściemnianie.

Ustawia jasność kanału.

Przykład: `55`

### Publikowane topiki

#### Stan jasności

- Topik: `{prefix}/channels/{channel}/state/brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące ściemnianie.

Bieżąca jasność ściemniacza.

Przykład: `42`

#### Stan włączenia

- Topik: `{prefix}/channels/{channel}/state/on`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące ściemnianie.

Bieżący stan włączenia lub wyłączenia.

Przykład: `true`

## Sterownik RGB

Typ kanału: `SUPLA_CHANNELTYPE_RGBLEDCONTROLLER`

Funkcja kanału: `SUPLA_CHANNELFNC_RGBLIGHTING`

### Subskrybowane topiki

#### Wykonaj akcję RGB

- Topik: `{prefix}/channels/{channel}/execute_action`
- Typ payloadu: `string`
- Dozwolone wartości: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące RGB.

Włącza, wyłącza lub przełącza bieżący stan wyjścia RGB. Wielkość liter nie ma znaczenia.

Przykład: `turn_on`

#### Ustaw kolor RGB

- Topik: `{prefix}/channels/{channel}/set/color`
- Typ payloadu: `string`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące RGB.

Ustawia kolor RGB jako rozdzielone przecinkami wartości czerwonej, zielonej i niebieskiej z zakresu 0..255.

Przykład: `1,2,3`

#### Ustaw jasność koloru

- Topik: `{prefix}/channels/{channel}/set/color_brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące RGB.

Ustawia jasność koloru RGB bez zmiany koloru.

Przykład: `44`

### Publikowane topiki

#### Kolor RGB

- Topik: `{prefix}/channels/{channel}/state/color`
- Typ payloadu: `string`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące RGB.

Bieżący kolor RGB jako rozdzielone przecinkami wartości czerwonej, zielonej i niebieskiej.

Przykład: `1,2,3`

#### Jasność koloru

- Topik: `{prefix}/channels/{channel}/state/color_brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące RGB.

Bieżąca jasność koloru RGB.

Przykład: `4`

#### Stan włączenia

- Topik: `{prefix}/channels/{channel}/state/on`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące RGB.

Bieżący stan włączenia lub wyłączenia.

Przykład: `true`

## Sterownik ściemniacza i RGB

Typ kanału: `SUPLA_CHANNELTYPE_DIMMERANDRGBLED`

Funkcja kanału: `SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING`

### Subskrybowane topiki

#### Wykonaj akcję ściemniacza

- Topik: `{prefix}/channels/{channel}/execute_action/dimmer`
- Typ payloadu: `string`
- Dozwolone wartości: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Dostępność: Połączone kanały ściemniacza i RGB.

Steruje częścią ściemniacza w połączonym kanale ściemniacza i RGB.

Przykład: `turn_on`

#### Wykonaj akcję RGB

- Topik: `{prefix}/channels/{channel}/execute_action/rgb`
- Typ payloadu: `string`
- Dozwolone wartości: `turn_on`, `turn_off`, `toggle`
- QoS: `0`
- Retain: `false`
- Dostępność: Połączone kanały ściemniacza i RGB.

Steruje częścią RGB w połączonym kanale ściemniacza i RGB.

Przykład: `turn_off`

#### Ustaw jasność

- Topik: `{prefix}/channels/{channel}/set/brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące ściemnianie.

Ustawia jasność kanału.

Przykład: `55`

#### Ustaw kolor RGB

- Topik: `{prefix}/channels/{channel}/set/color`
- Typ payloadu: `string`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące RGB.

Ustawia kolor RGB jako rozdzielone przecinkami wartości czerwonej, zielonej i niebieskiej z zakresu 0..255.

Przykład: `1,2,3`

#### Ustaw jasność koloru

- Topik: `{prefix}/channels/{channel}/set/color_brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały obsługujące RGB.

Ustawia jasność koloru RGB bez zmiany koloru.

Przykład: `44`

### Publikowane topiki

#### Stan jasności

- Topik: `{prefix}/channels/{channel}/state/brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące ściemnianie.

Bieżąca jasność ściemniacza.

Przykład: `8`

#### Kolor RGB

- Topik: `{prefix}/channels/{channel}/state/color`
- Typ payloadu: `string`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące RGB.

Bieżący kolor RGB jako rozdzielone przecinkami wartości czerwonej, zielonej i niebieskiej.

Przykład: `5,6,7`

#### Jasność koloru

- Topik: `{prefix}/channels/{channel}/state/color_brightness`
- Typ payloadu: `integer`
- Zakres: `0..100`
- Jednostka: `%`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały obsługujące RGB.

Bieżąca jasność koloru RGB.

Przykład: `11`

#### Stan włączenia ściemniacza

- Topik: `{prefix}/channels/{channel}/state/dimmer/on`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `true`
- Dostępność: Połączone kanały ściemniacza i RGB.

Bieżący stan włączenia części ściemniacza.

Przykład: `true`

#### Stan włączenia RGB

- Topik: `{prefix}/channels/{channel}/state/rgb/on`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `true`
- Dostępność: Połączone kanały ściemniacza i RGB.

Bieżący stan włączenia części RGB.

Przykład: `true`

## Termometr

Typ kanału: `SUPLA_CHANNELTYPE_THERMOMETER`

Funkcja kanału: `SUPLA_CHANNELFNC_NONE`

### Publikowane topiki

#### Temperatura

- Topik: `{prefix}/channels/{channel}/state/temperature`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `true`
- Dostępność: Czujniki temperatury z prawidłowym odczytem.

Bieżący odczyt temperatury.

Przykład: `21.50`

## Czujnik wilgotności i temperatury

Typ kanału: `SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR`

Funkcja kanału: `SUPLA_CHANNELFNC_NONE`

### Publikowane topiki

#### Wilgotność

- Topik: `{prefix}/channels/{channel}/state/humidity`
- Typ payloadu: `number`
- Jednostka: `%`
- QoS: `0`
- Retain: `true`
- Dostępność: Czujniki wilgotności i temperatury z prawidłowym odczytem wilgotności.

Bieżąca wilgotność względna.

Przykład: `55.00`

#### Temperatura

- Topik: `{prefix}/channels/{channel}/state/temperature`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `true`
- Dostępność: Czujniki temperatury z prawidłowym odczytem.

Bieżący odczyt temperatury.

Przykład: `21.50`

## HVAC

Typ kanału: `SUPLA_CHANNELTYPE_HVAC`

Funkcje kanału: `SUPLA_CHANNELFNC_HVAC_THERMOSTAT`, `SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL`

### Subskrybowane topiki

#### Wykonaj akcję HVAC

- Topik: `{prefix}/channels/{channel}/execute_action`
- Typ payloadu: `string`
- Dozwolone wartości: `turn_on`, `turn_off`, `off`, `toggle`, `auto`, `heat`, `cool`, `heat_cool`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały termostatów HVAC z funkcją grzania i chłodzenia.

Zmienia stan pracy lub tryb termostatu grzanie-chłodzenie. Wielkość liter nie ma znaczenia.

Przykład: `turn_on`

#### Ustaw temperaturę zadaną

- Topik: `{prefix}/channels/{channel}/set/temperature_setpoint`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały termostatów HVAC.

Ustawia aktywną temperaturę zadaną grzania lub chłodzenia zgodnie z konfiguracją kanału HVAC.

Przykład: `19.5`

#### Ustaw temperaturę zadaną chłodzenia

- Topik: `{prefix}/channels/{channel}/set/temperature_setpoint_cool`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały HVAC z temperaturą zadaną chłodzenia.

Ustawia temperaturę zadaną chłodzenia.

Przykład: `22.5`

#### Ustaw temperaturę zadaną grzania

- Topik: `{prefix}/channels/{channel}/set/temperature_setpoint_heat`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `false`
- Dostępność: Kanały HVAC z temperaturą zadaną grzania.

Ustawia temperaturę zadaną grzania.

Przykład: `18.5`

### Publikowane topiki

#### Akcja HVAC

- Topik: `{prefix}/channels/{channel}/state/action`
- Typ payloadu: `string`
- Dozwolone wartości: `off`, `idle`, `heating`, `cooling`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały HVAC.

Bieżąca akcja wykonywana przez sterownik HVAC.

Przykłady: `cooling`, `heating`, `idle`, `off`

#### Tryb HVAC

- Topik: `{prefix}/channels/{channel}/state/mode`
- Typ payloadu: `string`
- Dozwolone wartości: `off`, `auto`, `heat`, `cool`, `heat_cool`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały HVAC.

Bieżący tryb pracy HVAC.

Przykłady: `auto`, `cool`, `heat`, `heat_cool`, `off`

#### Temperatura zadana

- Topik: `{prefix}/channels/{channel}/state/temperature_setpoint`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje HVAC korzystające z jednej temperatury zadanej.

Bieżąca pojedyncza temperatura zadana HVAC.

Przykłady: `19.50`, `23.00`

#### Temperatura zadana chłodzenia

- Topik: `{prefix}/channels/{channel}/state/temperature_setpoint_cool`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje HVAC z oddzielnymi temperaturami zadanymi grzania i chłodzenia.

Bieżąca temperatura zadana chłodzenia.

Przykład: `23.00`

#### Temperatura zadana grzania

- Topik: `{prefix}/channels/{channel}/state/temperature_setpoint_heat`
- Typ payloadu: `number`
- Jednostka: `°C`
- QoS: `0`
- Retain: `true`
- Dostępność: Funkcje HVAC z oddzielnymi temperaturami zadanymi grzania i chłodzenia.

Bieżąca temperatura zadana grzania.

Przykład: `21.00`

## Licznik energii elektrycznej

Typ kanału: `SUPLA_CHANNELTYPE_ELECTRICITY_METER`

Funkcja kanału: `SUPLA_CHANNELFNC_ELECTRICITY_METER`

### Publikowane topiki

#### Sekwencja faz prądu

- Topik: `{prefix}/channels/{channel}/state/current_phase_sequence_clockwise`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące sekwencję faz prądu.

Informuje, czy sekwencja faz prądu jest zgodna z ruchem wskazówek zegara.

Przykład: `false`

#### Prąd fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/current`
- Typ payloadu: `number`
- Jednostka: `A`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące prąd poszczególnych faz.

Prąd zmierzony na jednej fazie.

Przykłady: `1.234`, `0.000`

#### Częstotliwość fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/frequency`
- Typ payloadu: `number`
- Jednostka: `Hz`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące częstotliwość poszczególnych faz.

Częstotliwość zmierzona na jednej fazie.

Przykład: `50.00`

#### Kąt fazowy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/phase_angle`
- Typ payloadu: `number`
- Jednostka: `°`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące kąty fazowe poszczególnych faz.

Kąt fazowy zmierzony na jednej fazie.

Przykłady: `12.3`, `0.0`

#### Moc czynna

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/power_active`
- Typ payloadu: `number`
- Jednostka: `W`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące moc czynną dla poszczególnych faz.

Bieżąca moc czynna jednej fazy.

Przykłady: `200.000`, `0.000`

#### Moc pozorna

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/power_apparent`
- Typ payloadu: `number`
- Jednostka: `VA`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące moc pozorną dla poszczególnych faz.

Bieżąca moc pozorna jednej fazy.

Przykłady: `0.000`, `0.300`

#### Współczynnik mocy fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/power_factor`
- Typ payloadu: `number`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące współczynnik mocy poszczególnych faz.

Współczynnik mocy zmierzony na jednej fazie.

Przykłady: `0.99`, `0.00`

#### Moc bierna

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/power_reactive`
- Typ payloadu: `number`
- Jednostka: `var`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące moc bierną dla poszczególnych faz.

Bieżąca moc bierna jednej fazy.

Przykłady: `0.000`, `0.100`

#### Pobrana energia czynna fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/total_forward_active_energy`
- Typ payloadu: `number`
- Jednostka: `kWh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące pobraną energię czynną dla poszczególnych faz.

Pobrana energia czynna dla jednej fazy.

Przykłady: `0.0100`, `0.0000`

#### Pobrana energia bierna fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/total_forward_reactive_energy`
- Typ payloadu: `number`
- Jednostka: `kvarh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące pobraną energię bierną dla poszczególnych faz.

Pobrana energia bierna dla jednej fazy.

Przykłady: `0.0300`, `0.0000`

#### Oddana energia czynna fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/total_reverse_active_energy`
- Typ payloadu: `number`
- Jednostka: `kWh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące oddaną energię czynną dla poszczególnych faz.

Oddana energia czynna dla jednej fazy.

Przykłady: `0.0200`, `0.0000`

#### Oddana energia bierna fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/total_reverse_reactive_energy`
- Typ payloadu: `number`
- Jednostka: `kvarh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące oddaną energię bierną dla poszczególnych faz.

Oddana energia bierna dla jednej fazy.

Przykłady: `0.0400`, `0.0000`

#### Napięcie fazy

- Topik: `{prefix}/channels/{channel}/state/phases/{phase}/voltage`
- Typ payloadu: `number`
- Jednostka: `V`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące napięcie poszczególnych faz.

Napięcie zmierzone na jednej fazie.

Przykłady: `230.00`, `0.00`

#### Całkowita pobrana energia czynna

- Topik: `{prefix}/channels/{channel}/state/total_forward_active_energy`
- Typ payloadu: `number`
- Jednostka: `kWh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące pobraną energię czynną.

Całkowita pobrana energia czynna ze wszystkich faz.

Przykład: `0.0100`

#### Całkowita zbilansowana pobrana energia czynna

- Topik: `{prefix}/channels/{channel}/state/total_forward_balanced_active_energy`
- Typ payloadu: `number`
- Jednostka: `kWh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące zbilansowaną pobraną energię czynną.

Całkowita pobrana energia czynna zbilansowana między fazami.

Przykład: `0.0300`

#### Całkowita oddana energia czynna

- Topik: `{prefix}/channels/{channel}/state/total_reverse_active_energy`
- Typ payloadu: `number`
- Jednostka: `kWh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące oddaną energię czynną.

Całkowita oddana energia czynna ze wszystkich faz.

Przykład: `0.0200`

#### Całkowita zbilansowana oddana energia czynna

- Topik: `{prefix}/channels/{channel}/state/total_reverse_balanced_active_energy`
- Typ payloadu: `number`
- Jednostka: `kWh`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące zbilansowaną oddaną energię czynną.

Całkowita oddana energia czynna zbilansowana między fazami.

Przykład: `0.0400`

#### Kąt fazowy napięcia 12

- Topik: `{prefix}/channels/{channel}/state/voltage_phase_angle_12`
- Typ payloadu: `number`
- Jednostka: `°`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące kąty fazowe napięcia.

Kąt fazowy napięcia między fazami 1 i 2.

Przykład: `120.0`

#### Kąt fazowy napięcia 13

- Topik: `{prefix}/channels/{channel}/state/voltage_phase_angle_13`
- Typ payloadu: `number`
- Jednostka: `°`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące kąty fazowe napięcia.

Kąt fazowy napięcia między fazami 1 i 3.

Przykład: `240.0`

#### Sekwencja faz napięcia

- Topik: `{prefix}/channels/{channel}/state/voltage_phase_sequence_clockwise`
- Typ payloadu: `boolean`
- Dozwolone wartości: `true`, `false`
- QoS: `0`
- Retain: `false`
- Dostępność: Liczniki energii raportujące sekwencję faz napięcia.

Informuje, czy sekwencja faz napięcia jest zgodna z ruchem wskazówek zegara.

Przykład: `true`

## Czujnik binarny

Typ kanału: `SUPLA_CHANNELTYPE_BINARYSENSOR`

Funkcje kanału: `SUPLA_CHANNELFNC_FLOOD_SENSOR`, `SUPLA_CHANNELFNC_OPENINGSENSOR_DOOR`

### Publikowane topiki

#### Stan czujnika binarnego

- Topik: `{prefix}/channels/{channel}/state`
- Typ payloadu: `string`
- Dozwolone wartości: `open`, `closed`, `ON`, `OFF`
- QoS: `0`
- Retain: `true`
- Dostępność: Kanały czujników binarnych ze skonfigurowaną funkcją.

Bieżący stan czujnika binarnego; używane wartości zależą od funkcji kanału.

Przykłady: `ON`, `closed`, `open`
