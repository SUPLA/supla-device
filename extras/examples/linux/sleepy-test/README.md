# SupLAN Sleepy Thermometer z AURATON Box

Source to standardowy sd4linux z jednym kanałem `ThermometerParsed`. Proces
startuje na nowo w każdym cyklu, odczytuje temperaturę z pliku i komunikuje
się przez SERVER SupLAN. Kolejne uruchomienia dają 20,0 → 20,5 → 21,0 → 21,5°C.
Domyślnie planowane starty dzieli 60 s, a aktywne okno po gotowości trwa 10 s.
W przerwie działa skrypt nadzorujący. Proces sd4linux i jego gniazda są zamknięte.

## Instalacja i pierwsza rejestracja

Binarka jest zbudowana dla Linux x86-64. Zobacz `BUILD_INFO.txt` po dokładne
wymagania ABI i sumę kontrolną. Przykład dla Ubuntu 24.04 amd64:

```bash
sudo apt install bash util-linux coreutils grep sed mawk libyaml-cpp0.8 libssl3t64 libstdc++6 ca-certificates
cd suplan-sleepy-test
cp config/supla-device.yaml.example config/supla-device.yaml
cp config/sleepy.conf.example config/sleepy.conf
${EDITOR:-vi} config/supla-device.yaml
```

Wpisz **adres serwera urządzeń** dla konta, e-mail konta i zachowaj `proto: 29`.
Wzorzec korzysta z walidacji certyfikatu SUPLA. Umieść Linux i Box w tej samej
sieci IPv4 z ruchem między klientami, UDP 2016 oraz multicastem
`239.255.201.6:2016`. Dla maszyny z kilkoma interfejsami można wskazać
`suplan.bind_address` jako jej adres IPv4 na wspólnej sieci. Użyj jednego
nadzorcy na katalog i osobnego katalogu stanu dla każdego urządzenia.

1. W Cloud włącz rejestrację urządzeń. Zarejestruj Box, jeśli jeszcze go tam nie ma.
2. Uruchom `./scripts/run.sh provision` na Linuxie. Ten tryb utrzymuje Source
   online z wartością 20,0°C. Nadzorca wypisze ścieżkę logu.
3. W Cloud znajdź urządzenie **SupLAN Sleepy Thermometer**. Sprawdź, że jego
   kanał ma funkcję **Termometr**. Zapisz ID kanału. Oba urządzenia powinny
   zgłaszać obsługę SupLAN i należeć do tego samego konta.
4. W konfiguracji kanału termostatu Boxa wybierz **Główny termometr**
   (`mainThermometerChannelId`) z grupy kanałów zdalnych i wskaż ten kanał
   sd4linux. Zapisz zmianę, gdy oba urządzenia są online. Taka relacja HVAC
   w Cloud tworzy grant CHANNEL READ automatycznie. Server dostarcza
   asocjacje Source i Destination obu urządzeniom. Pozostaw oba online
   do pierwszego odczytu temperatury przez termostat. W logu Source szukaj
   `Received DEVICE_SYNC_DONE` oraz
   `association/rights changed ... role=Source ... result=OK`.
5. Potwierdź na **kanale termostatu Boxa** aktualną temperaturę 20,0°C.
   Zatrzymaj tryb ciągły poleceniem `./scripts/stop.sh` z drugiego terminala
   albo Ctrl+C w terminalu trybu ciągłego.

Wymagany jest build Boxa, który faktycznie uruchamia obsługę SupLAN przy
rejestracji protokołem 29, odczytuje główny termometr z innego urządzenia i
obsługuje authenticated SOURCE_INTEREST_RESET, PEER_AWAKE_HINT oraz
oczekiwany sen z niezależną ważnością snapshotu. Sam numer protokołu 29
nie potwierdza tych funkcji. Dla aktualnie wgranego Boxa zapisz wersję firmware
i, jeśli jest dostępny, commit SDK z manifestu kompilacji. Brak pozycji
zdalnego kanału w Cloud diagnozuj najpierw po `suplanSupported` obu urządzeń,
funkcji kanału i możliwości zmiany pola głównego termometru. Komunikat
`Box: SupLAN disabled for this boot` lub `SupLAN enable failed` w logu
startowym oznacza, że ten start nie nadaje się do testu.

## Cykle i obserwacja Boxa

```bash
./scripts/run.sh normal             # cykle do zatrzymania
./scripts/status.sh                # w drugim terminalu
./scripts/stop.sh
./scripts/run.sh kill 4             # 4 planowane sloty, SIGKILL w domyślnym slocie 2
./scripts/run.sh skip 6             # 6 slotów, domyślnie pomija sloty 2–4
```

`config/sleepy.conf` ustawia okres, budżet gotowości, aktywne okno, czas
na końcowe retransmisje oraz sloty testu awarii. Sloty liczą się od startu
nadzorcy. Temperatura przesuwa się tylko po uruchomieniu procesu, również
po cyklu zakończonym SIGKILL. `runtime/cycle` należy do aplikacji testowej.
`runtime/sdk/` należy do SDK i przechowuje m.in. `guid_auth.yaml`,
`config_storage.bin` i trwałe odpowiedzi SERVER. Trzymaj ten katalog przy
wszystkich ponownych uruchomieniach. Ponowny provisioning jest potrzebny
po zmianie konta lub tożsamości serwera. Kopię stanu rób przy zatrzymanym
nadzorcy i przechowuj prywatnie.

Zapisuj godzinę, numer cyklu, temperaturę mierzoną przez termostat, jej
dostępność oraz, jeśli masz dostęp, log Boxa. Przy kolejnych wake szukaj
20,0 → 20,5 → 21,0 → 21,5 → 20,0°C na termostacie bez ponownego zapisu
konfiguracji. Jeśli wskazanie Cloud jest niejednoznaczne, użyj istniejącej
diagnostyki Boxa i koreluj odbiór snapshotu temperatury z ID termometru
oraz odczytem termostatu. Wartość wyświetlana na karcie **Source** potwierdza
pomiar Source. Dowodem użycia przez Box jest pomiar na **termostacie**.

Po zapowiedzianym uśpieniu bieżący snapshot może pozostawać ważny. SDK
domyślnie stosuje w D2D 180 s od odbioru snapshotu. Pomijając 3 domyślne
sloty, można obserwować wygaśnięcie pomiaru, a potem wznowienie. W trybie
`kill` oczekuj kodu 137 i braku `ANNOUNCE_SLEEP` w zabitym cyklu. SDK
odbudowuje READ automatycznie na bazie obowiązującego żądania termostatu.
Po utracie resetu zdrowy okresowy READ jest ścieżką awaryjną. Pojedyncze
krótkie okno po utracie hintów może nie pokryć się z terminem naprawy.

## Logi i testy

```bash
./scripts/collect-logs.sh /tmp/codex/sleepy-logs-20261123
./scripts/smoke-test.sh
```

Zbieranie kopiuje logi Source i wylicza podsumowanie. Podaj nową, bezwzględną
ścieżkę, której katalog nadrzędny istnieje. `SERVER_READY source=live_sync`
oznacza aktualną synchronizację SERVER. `source=durable_identity_and_grant`
oznacza gotowość D2D z trwałych danych. `UDP_READY` wymaga otwarcia UDP oraz
dołączenia do grupy multicast. `BEGIN_WAKE` używa `retained_interests=false`
po każdym restarcie procesu. `PEER_AWAKE` i `INTEREST_RESET` oznaczają
datagram zaakceptowany przez lokalny transport. SDK może wysłać także
startupowy reset przed `BEGIN_WAKE`. `READ_RECEIVED` mierzy READ przyjęty
przez Runtime. `STATE_SENT` mierzy wysłanie snapshotu Source. Porównaj je z
obserwacją na Boxie. `ANNOUNCE_SLEEP initial_datagrams=0` oznacza brak
pierwszego datagramu na UDP, np. brak sesji. Po końcowym oknie
`SLEEP_RESULT sent=1` potwierdza, że lokalne UDP przyjęło zapowiedź
przy pierwszej próbie lub retransmisji. Kod 3 w `CYCLE_RESULT` oznacza
nieudaną synchronizację Source lub brak wysłanej zapowiedzi snu. Kod 124 oznacza
zadziałanie watchdoga nadzorcy. Każdy cykl ma identyfikator, czas Unix oraz
w logach SLEEPY monotoniczny czas od startu procesu. Do głębszej diagnostyki
ustaw `log_level: debug` w YAML. Zbieraj logi aplikacji i odbiorcy, pomijając
przy udostępnianiu katalog `runtime/sdk/` oraz prywatny YAML konta.

`smoke-test.sh` uruchamia osobny katalog pod `/tmp/codex` i sprawdza
integrację skryptów z binarką, kolejność temperatur, timeout, stop, SIGKILL,
skip i trwałość GUID/AUTHKEY. Sprawdzenie wartości na drugim sd4linux wymaga
prawdziwego Cloud/Server i kanału `Hvac` z przypisanym Source. Przykład
Destination jest w `config/linux-destination.yaml.example`. Utwórz
`destination-sdk/` oraz lokalny plik pomiaru (`printf '14.0\n' > local-temperature.txt`),
wpisz adres Server i e-mail, a następnie uruchom
Destination z osobną konfiguracją i `--debug-socket /ścieżka/do/destination.sock`
w prywatnym katalogu. Kanał HVAC w przykładzie ma numer lokalny 1. Na tym samym hoście użyj osobnych przestrzeni sieciowych w jednej
sieci multicast. Po provisioningu i zatrzymaniu ciągłego Source uruchom
obserwator, który sam wystartuje nadzorcę i sprawdzi pomiar HVAC, sesje i RSS.
W trybach awarii podaj liczbę slotów obejmującą pełną zaplanowaną awarię
oraz co najmniej jeden zwykły cykl przed nią i po niej. Weryfikator sprawdza
ciszę odbiorcy w trakcie zapowiedzianego snu i raportuje odstęp między
próbkami dla każdego sprawdzonego slotu. Wymaga Python 3 oraz dostępu do
`/proc` odbiorcy. Dodaj
`--source-udp-port 2016`, gdy obserwator działa w izolowanej przestrzeni
sieciowej Source, aby sprawdzić także zamknięcie UDP w czasie snu.

```bash
./scripts/verify-linux.py /ścieżka/do/destination.sock ID_TERMOMETRU NR_KANAŁU_HVAC PID_DEST
./scripts/verify-linux.py /ścieżka/do/destination.sock ID_TERMOMETRU NR_KANAŁU_HVAC PID_DEST --mode kill --slots 3
./scripts/verify-linux.py /ścieżka/do/destination.sock ID_TERMOMETRU NR_KANAŁU_HVAC PID_DEST --mode skip --slots 5 --expect-expiry
```

## Lokalna kompilacja na innej architekturze lub dystrybucji

Na Ubuntu/Debian zainstaluj `cmake`, `g++`, `make`, `git`, `libssl-dev`
i `libyaml-cpp-dev`. Pierwsza konfiguracja pobiera przez HTTPS przypięte
wersje nlohmann/json, cxxopts i MQTT-C. Uruchom poniższe z katalogu pakietu.

```bash
mkdir -p /tmp/codex
build_dir=$(mktemp -d /tmp/codex/suplan-sleepy-build.XXXXXXXX)
tar -xf suplan-sleepy-source.tar.gz -C "$build_dir"
cmake -S "$build_dir/extras/examples/linux" -B "$build_dir/build" \
    -DCMAKE_BUILD_TYPE=Release -DSUPLA_LINUX_ENABLE_HTTP_SOURCE=OFF
cmake --build "$build_dir/build" -j 4
install -m 0755 "$build_dir/build/supla-device-linux" bin/supla-device-linux
```
