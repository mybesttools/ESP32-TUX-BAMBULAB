/*
MIT License

Copyright (c) 2024 ESP32-TUX Contributors
*/

#include "WebServer.hpp"
#include "SettingsConfig.hpp"
#include "PrinterDiscovery.hpp"
#include <cstring>
#include <esp_log.h>
#include <cJSON.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_app_desc.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include "tux_events.hpp"

static const char *TAG = "WebServer";

WebServer *web_server = nullptr;
extern SettingsConfig *cfg;

// Static member initialization
std::vector<std::string> WebServer::g_discovered_ips;
bool WebServer::g_discovery_in_progress = false;
int WebServer::g_discovery_progress = 0;
TaskHandle_t WebServer::g_discovery_task = nullptr;
static SemaphoreHandle_t g_discovered_ips_mutex = nullptr;

// HTML page served at root
static const char *HTML_PAGE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-TUX Configuration</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial; margin: 0; padding: 20px; background: #1e1e1e; color: #fff; }
        .container { max-width: 800px; margin: 0 auto; }
        h1 { color: #00bfff; text-align: center; }
        .panel { background: #2d2d2d; border: 1px solid #00bfff; border-radius: 5px; padding: 15px; margin: 15px 0; }
        label { display: block; margin: 10px 0 5px 0; font-weight: bold; }
        input, select, textarea { width: 100%; padding: 8px; margin-bottom: 10px; 
                                  background: #3a3a3a; color: #fff; border: 1px solid #00bfff; 
                                  border-radius: 3px; box-sizing: border-box; }
        button { background: #00bfff; color: #000; padding: 10px 20px; border: none; 
                border-radius: 3px; cursor: pointer; font-weight: bold; margin: 5px; }
        button:hover { background: #00d4ff; }
        .button-group { display: flex; gap: 10px; flex-wrap: wrap; }
        .status { padding: 10px; margin: 10px 0; border-radius: 3px; }
        .success { background: #4a9d6f; }
        .error { background: #c9515d; }
        .printer-list { list-style: none; padding: 0; }
        .printer-item { background: #3a3a3a; padding: 10px; margin: 5px 0; border-left: 3px solid #00bfff; }
        h2 { color: #00bfff; margin-top: 0; }
        hr { border: 1px solid #00bfff; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🎨 ESP32-TUX Configuration</h1>
        
        <!-- Settings Panel -->
        <div class="panel">
            <h2 data-i18n="systemSettings">⚙️ System Settings</h2>
            <label data-i18n="brightness">Brightness:</label>
            <input type="range" id="brightness" min="50" max="255" value="128">
            <span id="brightnessVal">128</span>
            
            <label data-i18n="theme">Theme:</label>
            <select id="theme">
                <option value="dark">Dark</option>
                <option value="light">Light</option>
            </select>
            
            <label data-i18n="timezone">Timezone:</label>
            <select id="timezone">
                <option value="" disabled selected hidden>-- Select Timezone --</option>
                <option value="UTC0">UTC (GMT)</option>
                <optgroup label="Europe">
                    <option value="GMT0">London (GMT)</option>
                    <option value="CET-1CEST,M3.5.0,M10.5.0">Central Europe (CET)</option>
                    <option value="WET0WEST,M3.5.0,M10.5.0">Western Europe (WET)</option>
                    <option value="EET-2EEST,M3.5.0,M10.5.0">Eastern Europe (EET)</option>
                    <option value="WEST1WEAST,M3.5.0,M10.5.0">Portugal (WET)</option>
                </optgroup>
                <optgroup label="Americas">
                    <option value="EST5EDT,M3.2.0,M11.1.0">US Eastern (EST)</option>
                    <option value="CST6CDT,M3.2.0,M11.1.0">US Central (CST)</option>
                    <option value="MST7MDT,M3.2.0,M11.1.0">US Mountain (MST)</option>
                    <option value="PST8PDT,M3.2.0,M11.1.0">US Pacific (PST)</option>
                    <option value="AKST9AKDT,M3.2.0,M11.1.0">Alaska (AKST)</option>
                    <option value="HST10">Hawaii (HST)</option>
                </optgroup>
                <optgroup label="Asia">
                    <option value="IST-5:30">India (IST)</option>
                    <option value="CST-8">China (CST)</option>
                    <option value="JST-9">Japan (JST)</option>
                    <option value="KST-9">Korea (KST)</option>
                    <option value="SGT-8">Singapore (SGT)</option>
                    <option value="AEST-10AEDT,M10.1.0,M4.1.0">Sydney (AEST)</option>
                    <option value="NZST-12NZDT,M9.5.0,M4.1.0">New Zealand (NZST)</option>
                </optgroup>
                <optgroup label="Other">
                    <option value="SAST-2">South Africa (SAST)</option>
                    <option value="AEST-10">Perth (AEST)</option>
                </optgroup>
            </select>
            
            <label data-i18n="language">Language:</label>
            <select id="language">
                <option value="en" selected>English</option>
                <option value="de">Deutsch (German)</option>
                <option value="nl">Nederlands (Dutch)</option>
                <option value="pl">Polski (Polish)</option>
                <option value="ru">Русский (Russian - requires font update)</option>
            </select>
            
            <div class="button-group">
                <button onclick="saveSettings()" data-i18n="saveSettings">💾 Save Settings</button>
                <button onclick="loadSettings()" data-i18n="loadSettings">🔄 Load Settings</button>
            </div>
            <div id="settingsStatus"></div>
        </div>
        
        <!-- Weather Panel -->
        <div class="panel">
            <h2 data-i18n="weatherSettings">🌤️ Weather Settings</h2>
            <form onsubmit="return false;">
            <label data-i18n="apiKey">API Key:</label>
            <input type="password" id="apiKey" data-i18n="apiKeyPlaceholder" placeholder="Enter OpenWeatherMap API key">
            </form>
            
            <div class="button-group">
                <button onclick="saveWeatherSettings()" data-i18n="saveWeatherSettings">💾 Save Weather Settings</button>
                <button onclick="loadWeatherSettings()" data-i18n="loadWeatherSettings">🔄 Load Weather Settings</button>
            </div>
            <div id="weatherStatus"></div>
        </div>
        
        <!-- Weather Locations Panel -->
        <div class="panel">
            <h2 data-i18n="manageWeatherLocations">📍 Manage Weather Locations</h2>
            
            <h3 data-i18n="addLocation">Add Location</h3>
            <label data-i18n="locationName">Location Name:</label>
            <input type="text" id="locationName" data-i18n="locationNamePlaceholder" placeholder="e.g., Home, Office">
            
            <label data-i18n="city">City:</label>
            <input type="text" id="locationCity" data-i18n="cityPlaceholder" placeholder="e.g., Kleve">
            
            <label data-i18n="country">Country:</label>
            <input type="text" id="locationCountry" data-i18n="countryPlaceholder" placeholder="e.g., Germany">
            
            <label data-i18n="latitude">Latitude:</label>
            <input type="number" id="locationLat" placeholder="51.7934" step="0.0001">
            
            <label data-i18n="longitude">Longitude:</label>
            <input type="number" id="locationLon" placeholder="6.1368" step="0.0001">
            
            <div class="button-group">
                <button onclick="addWeatherLocation()" data-i18n="addLocationBtn">➕ Add Location</button>
            </div>
            
            <h3 data-i18n="configuredLocations">Configured Locations</h3>
            <ul class="printer-list" id="locationList"></ul>
            <div id="locationStatus"></div>
        </div>
        
        <!-- Printers Panel -->
        <div class="panel">
            <h2 data-i18n="printerConfiguration">🖨️ Printer Configuration</h2>
            
            <h3 data-i18n="autoDiscover">Auto-Discover Printers</h3>
            <p style="font-size: 12px; color: #aaaaaa;" data-i18n="autoDiscoverDesc">Searches for Bambu Lab printers on your network</p>
            <div class="button-group">
                <button onclick="discoverPrinters()" data-i18n="discoverPrintersBtn">🔍 Discover Printers</button>
            </div>
            <div id="discoverStatus"></div>
            
            <!-- Discovered printers dropdown -->
            <div id="discoveredPrinterSection" style="display:none; margin-top: 15px;">
                <label data-i18n="selectDiscovered">Select from discovered printers:</label>
                <select id="discoveredPrinterDropdown" onchange="selectDiscoveredPrinter()">
                    <option value="">-- Choose a printer --</option>
                </select>
            </div>
            
            <hr>
            
            <h3 data-i18n="addManually">Add Printer Manually</h3>
            <label data-i18n="printerName">Printer Name:</label>
            <input type="text" id="printerName" data-i18n="printerNamePlaceholder" placeholder="e.g., Bambu Lab X1" oninput="validatePrinterForm()">
            
            <label data-i18n="ipAddress">IP Address:</label>
            <input type="text" id="printerIP" data-i18n="ipPlaceholder" placeholder="192.168.1.100" oninput="validatePrinterForm()">
            
            <label data-i18n="printerCode">Printer Code:</label>
            <input type="password" id="printerToken" data-i18n="printerCodePlaceholder" placeholder="Enter printer access code" oninput="validatePrinterForm()">
            
            <label style="display: flex; align-items: center; gap: 8px; cursor: pointer;">
                <input type="checkbox" id="disableSslVerify" checked>
                <span>Disable SSL verification (recommended for easier setup)</span>
            </label>
            <p style="font-size: 11px; color: #aaa; margin: -5px 0 10px 28px;">⚠️ Disabling SSL verification is less secure but avoids certificate setup. Only use on trusted networks.</p>
            
            <label data-i18n="serialNumber">Serial Number:</label>
            <div style="display: flex; gap: 8px;">
                <input type="text" id="printerSerial" data-i18n="serialPlaceholder" placeholder="e.g., 0309DA541804686 (REQUIRED for A1 Mini)" style="flex: 1;">
                <button id="fetchSerialBtn" onclick="fetchPrinterSerial()" disabled>🔍 Fetch Serial</button>
            </div>
            <p style="font-size: 11px; color: #f0ad4e; margin: -5px 0 10px 0;" data-i18n="a1MiniSerialWarning">⚠️ A1 Mini REQUIRES serial number. Find it in Bambu app → Device → Settings → Device Info</p>
            
            <div class="button-group">
                <button id="addPrinterBtn" onclick="addPrinter()" data-i18n="addPrinterBtn" disabled>➕ Add Printer</button>
            </div>
            
            <h3 data-i18n="configuredPrinters">Configured Printers</h3>
            <ul class="printer-list" id="printerList"></ul>
            <div id="printerStatus"></div>
        </div>
        
        <!-- Networks Panel -->
        <div class="panel">
            <h2>🌐 Discovery Networks</h2>
            <p style="font-size: 0.9em; color: #999;">Configure additional networks to scan for printers (e.g., Guest networks, other subnets)</p>
            
            <label>Network Name:</label>
            <input type="text" id="networkName" placeholder="e.g., Guest Network, Office">
            
            <label>Subnet (CIDR):</label>
            <input type="text" id="networkSubnet" placeholder="e.g., 192.168.1.0/24 or 10.0.0.0/24">
            
            <div class="button-group">
                <button onclick="addNetwork()">➕ Add Network</button>
            </div>
            
            <h3>Configured Networks</h3>
            <ul class="printer-list" id="networkList"></ul>
            <div id="networkStatus"></div>
        </div>
        
        <!-- Device Info Panel -->
        <div class="panel">
            <h2>ℹ️ Device Information</h2>
            <div id="deviceInfo" style="background: #3a3a3a; padding: 10px; border-radius: 3px; 
                                        font-family: monospace; white-space: pre-wrap; word-wrap: break-word;"></div>
            <button onclick="loadDeviceInfo()">🔄 Refresh</button>
        </div>
    </div>

    <script>
        // i18n Translation System
        const translations = {
            en: {
                title: 'ESP32-TUX Configuration',
                systemSettings: 'System Settings',
                brightness: 'Brightness',
                theme: 'Theme',
                themeDark: 'Dark',
                themeLight: 'Light',
                timezone: 'Timezone',
                selectTimezone: '-- Select Timezone --',
                language: 'Language',
                saveSettings: 'Save Settings',
                loadSettings: 'Load Settings',
                weatherSettings: 'Weather Settings',
                apiKey: 'API Key',
                apiKeyPlaceholder: 'Enter OpenWeatherMap API key',
                saveWeatherSettings: 'Save Weather Settings',
                loadWeatherSettings: 'Load Weather Settings',
                manageWeatherLocations: 'Manage Weather Locations',
                addLocation: 'Add Location',
                locationName: 'Location Name',
                locationNamePlaceholder: 'e.g., Home, Office',
                city: 'City',
                cityPlaceholder: 'e.g., Kleve',
                country: 'Country',
                countryPlaceholder: 'e.g., Germany',
                latitude: 'Latitude',
                longitude: 'Longitude',
                addLocationBtn: 'Add Location',
                configuredLocations: 'Configured Locations',
                printerConfiguration: 'Printer Configuration',
                autoDiscover: 'Auto-Discover Printers',
                autoDiscoverDesc: 'Searches for Bambu Lab printers on your network',
                discoverPrintersBtn: 'Discover Printers',
                selectDiscovered: 'Select from discovered printers',
                choosePrinter: '-- Choose a printer --',
                addManually: 'Add Printer Manually',
                printerName: 'Printer Name',
                printerNamePlaceholder: 'e.g., Bambu Lab X1',
                ipAddress: 'IP Address',
                ipPlaceholder: '192.168.1.100',
                printerCode: 'Printer Code',
                printerCodePlaceholder: 'Enter printer access code',
                serialNumber: 'Serial Number',
                serialPlaceholder: 'Enter printer serial number',
                addPrinterBtn: 'Add Printer',
                configuredPrinters: 'Configured Printers',
                deleteBtn: 'Delete',
                settingsSaved: 'Settings saved!',
                weatherSettingsSaved: 'Weather settings saved!',
                locationAdded: 'Location added!',
                printerAdded: 'Printer added!',
                error: 'Error',
                errorLoading: 'Error loading',
                scanning: 'Scanning',
                starting: 'Starting',
                noDiscoveredPrinters: 'No printers discovered',
                discoveryComplete: 'Discovery complete',
                a1MiniSerialWarning: '⚠️ A1 Mini REQUIRES serial number. Find it in Bambu app → Device → Settings → Device Info'
            },
            de: {
                title: 'ESP32-TUX Konfiguration',
                systemSettings: 'Systemeinstellungen',
                brightness: 'Helligkeit',
                theme: 'Thema',
                themeDark: 'Dunkel',
                themeLight: 'Hell',
                timezone: 'Zeitzone',
                selectTimezone: '-- Zeitzone wählen --',
                language: 'Sprache',
                saveSettings: 'Einstellungen speichern',
                loadSettings: 'Einstellungen laden',
                weatherSettings: 'Wettereinstellungen',
                apiKey: 'API-Schlüssel',
                apiKeyPlaceholder: 'OpenWeatherMap API-Schlüssel eingeben',
                saveWeatherSettings: 'Wettereinstellungen speichern',
                loadWeatherSettings: 'Wettereinstellungen laden',
                manageWeatherLocations: 'Wetter-Standorte verwalten',
                addLocation: 'Standort hinzufügen',
                locationName: 'Standortname',
                locationNamePlaceholder: 'z.B. Zuhause, Büro',
                city: 'Stadt',
                cityPlaceholder: 'z.B. Kleve',
                country: 'Land',
                countryPlaceholder: 'z.B. Deutschland',
                latitude: 'Breitengrad',
                longitude: 'Längengrad',
                addLocationBtn: 'Standort hinzufügen',
                configuredLocations: 'Konfigurierte Standorte',
                printerConfiguration: 'Druckerkonfiguration',
                autoDiscover: 'Drucker automatisch erkennen',
                autoDiscoverDesc: 'Sucht nach Bambu Lab Druckern in Ihrem Netzwerk',
                discoverPrintersBtn: 'Drucker suchen',
                selectDiscovered: 'Aus erkannten Druckern auswählen',
                choosePrinter: '-- Drucker auswählen --',
                addManually: 'Drucker manuell hinzufügen',
                printerName: 'Druckername',
                printerNamePlaceholder: 'z.B. Bambu Lab X1',
                ipAddress: 'IP-Adresse',
                ipPlaceholder: '192.168.1.100',
                printerCode: 'Druckercode',
                printerCodePlaceholder: 'Drucker-Zugangscode eingeben',
                serialNumber: 'Seriennummer',
                serialPlaceholder: 'Drucker-Seriennummer eingeben',
                addPrinterBtn: 'Drucker hinzufügen',
                configuredPrinters: 'Konfigurierte Drucker',
                deleteBtn: 'Löschen',
                settingsSaved: 'Einstellungen gespeichert!',
                weatherSettingsSaved: 'Wettereinstellungen gespeichert!',
                locationAdded: 'Standort hinzugefügt!',
                printerAdded: 'Drucker hinzugefügt!',
                error: 'Fehler',
                errorLoading: 'Fehler beim Laden',
                scanning: 'Scanne',
                starting: 'Starte',
                noDiscoveredPrinters: 'Keine Drucker gefunden',
                discoveryComplete: 'Suche abgeschlossen',
                a1MiniSerialWarning: '⚠️ A1 Mini ERFORDERT Seriennummer. Finden Sie diese in der Bambu App → Gerät → Einstellungen → Geräteinformationen'
            },
            nl: {
                title: 'ESP32-TUX Configuratie',
                systemSettings: 'Systeeminstellingen',
                brightness: 'Helderheid',
                theme: 'Thema',
                themeDark: 'Donker',
                themeLight: 'Licht',
                timezone: 'Tijdzone',
                selectTimezone: '-- Selecteer tijdzone --',
                language: 'Taal',
                saveSettings: 'Instellingen opslaan',
                loadSettings: 'Instellingen laden',
                weatherSettings: 'Weerinstellingen',
                apiKey: 'API-sleutel',
                apiKeyPlaceholder: 'Voer OpenWeatherMap API-sleutel in',
                saveWeatherSettings: 'Weerinstellingen opslaan',
                loadWeatherSettings: 'Weerinstellingen laden',
                manageWeatherLocations: 'Weerlocaties beheren',
                addLocation: 'Locatie toevoegen',
                locationName: 'Locatienaam',
                locationNamePlaceholder: 'bijv. Thuis, Kantoor',
                city: 'Stad',
                cityPlaceholder: 'bijv. Amsterdam',
                country: 'Land',
                countryPlaceholder: 'bijv. Nederland',
                latitude: 'Breedtegraad',
                longitude: 'Lengtegraad',
                addLocationBtn: 'Locatie toevoegen',
                configuredLocations: 'Geconfigureerde locaties',
                printerConfiguration: 'Printerconfiguratie',
                autoDiscover: 'Printers automatisch detecteren',
                autoDiscoverDesc: 'Zoekt naar Bambu Lab printers op uw netwerk',
                discoverPrintersBtn: 'Printers zoeken',
                selectDiscovered: 'Selecteer uit ontdekte printers',
                choosePrinter: '-- Kies een printer --',
                addManually: 'Printer handmatig toevoegen',
                printerName: 'Printernaam',
                printerNamePlaceholder: 'bijv. Bambu Lab X1',
                ipAddress: 'IP-adres',
                ipPlaceholder: '192.168.1.100',
                printerCode: 'Printercode',
                printerCodePlaceholder: 'Voer printer toegangscode in',
                serialNumber: 'Serienummer',
                serialPlaceholder: 'Voer printer serienummer in',
                addPrinterBtn: 'Printer toevoegen',
                configuredPrinters: 'Geconfigureerde printers',
                deleteBtn: 'Verwijderen',
                settingsSaved: 'Instellingen opgeslagen!',
                weatherSettingsSaved: 'Weerinstellingen opgeslagen!',
                locationAdded: 'Locatie toegevoegd!',
                printerAdded: 'Printer toegevoegd!',
                error: 'Fout',
                errorLoading: 'Fout bij laden',
                scanning: 'Scannen',
                starting: 'Starten',
                noDiscoveredPrinters: 'Geen printers gevonden',
                discoveryComplete: 'Zoeken voltooid',
                a1MiniSerialWarning: '⚠️ A1 Mini VEREIST serienummer. Vind het in de Bambu app → Apparaat → Instellingen → Apparaatinfo'
            },
            pl: {
                title: 'Konfiguracja ESP32-TUX',
                systemSettings: 'Ustawienia systemu',
                brightness: 'Jasność',
                theme: 'Motyw',
                themeDark: 'Ciemny',
                themeLight: 'Jasny',
                timezone: 'Strefa czasowa',
                selectTimezone: '-- Wybierz strefę czasową --',
                language: 'Język',
                saveSettings: 'Zapisz ustawienia',
                loadSettings: 'Wczytaj ustawienia',
                weatherSettings: 'Ustawienia pogody',
                apiKey: 'Klucz API',
                apiKeyPlaceholder: 'Wprowadź klucz API OpenWeatherMap',
                saveWeatherSettings: 'Zapisz ustawienia pogody',
                loadWeatherSettings: 'Wczytaj ustawienia pogody',
                manageWeatherLocations: 'Zarządzaj lokalizacjami pogody',
                addLocation: 'Dodaj lokalizację',
                locationName: 'Nazwa lokalizacji',
                locationNamePlaceholder: 'np. Dom, Biuro',
                city: 'Miasto',
                cityPlaceholder: 'np. Warszawa',
                country: 'Kraj',
                countryPlaceholder: 'np. Polska',
                latitude: 'Szerokość geograficzna',
                longitude: 'Długość geograficzna',
                addLocationBtn: 'Dodaj lokalizację',
                configuredLocations: 'Skonfigurowane lokalizacje',
                printerConfiguration: 'Konfiguracja drukarki',
                autoDiscover: 'Automatyczne wykrywanie drukarek',
                autoDiscoverDesc: 'Wyszukuje drukarki Bambu Lab w Twojej sieci',
                discoverPrintersBtn: 'Wykryj drukarki',
                selectDiscovered: 'Wybierz z wykrytych drukarek',
                choosePrinter: '-- Wybierz drukarkę --',
                addManually: 'Dodaj drukarkę ręcznie',
                printerName: 'Nazwa drukarki',
                printerNamePlaceholder: 'np. Bambu Lab X1',
                ipAddress: 'Adres IP',
                ipPlaceholder: '192.168.1.100',
                printerCode: 'Kod drukarki',
                printerCodePlaceholder: 'Wprowadź kod dostępu drukarki',
                serialNumber: 'Numer seryjny',
                serialPlaceholder: 'Wprowadź numer seryjny drukarki',
                addPrinterBtn: 'Dodaj drukarkę',
                configuredPrinters: 'Skonfigurowane drukarki',
                deleteBtn: 'Usuń',
                settingsSaved: 'Ustawienia zapisane!',
                weatherSettingsSaved: 'Ustawienia pogody zapisane!',
                locationAdded: 'Lokalizacja dodana!',
                printerAdded: 'Drukarka dodana!',
                error: 'Błąd',
                errorLoading: 'Błąd wczytywania',
                scanning: 'Skanowanie',
                starting: 'Uruchamianie',
                noDiscoveredPrinters: 'Nie znaleziono drukarek',
                discoveryComplete: 'Wykrywanie zakończone',
                a1MiniSerialWarning: '⚠️ A1 Mini WYMAGA numeru seryjnego. Znajdź go w aplikacji Bambu → Urządzenie → Ustawienia → Informacje o urządzeniu'
            },
            ru: {
                title: 'Конфигурация ESP32-TUX',
                systemSettings: 'Системные настройки',
                brightness: 'Яркость',
                theme: 'Тема',
                themeDark: 'Тёмная',
                themeLight: 'Светлая',
                timezone: 'Часовой пояс',
                selectTimezone: '-- Выберите часовой пояс --',
                language: 'Язык',
                saveSettings: 'Сохранить настройки',
                loadSettings: 'Загрузить настройки',
                weatherSettings: 'Настройки погоды',
                apiKey: 'API ключ',
                apiKeyPlaceholder: 'Введите ключ API OpenWeatherMap',
                saveWeatherSettings: 'Сохранить настройки погоды',
                loadWeatherSettings: 'Загрузить настройки погоды',
                manageWeatherLocations: 'Управление местоположениями погоды',
                addLocation: 'Добавить местоположение',
                locationName: 'Название местоположения',
                locationNamePlaceholder: 'например, Дом, Офис',
                city: 'Город',
                cityPlaceholder: 'например, Москва',
                country: 'Страна',
                countryPlaceholder: 'например, Россия',
                latitude: 'Широта',
                longitude: 'Долгота',
                addLocationBtn: 'Добавить местоположение',
                configuredLocations: 'Настроенные местоположения',
                printerConfiguration: 'Конфигурация принтера',
                autoDiscover: 'Автоопределение принтеров',
                autoDiscoverDesc: 'Поиск принтеров Bambu Lab в вашей сети',
                discoverPrintersBtn: 'Найти принтеры',
                selectDiscovered: 'Выбрать из найденных принтеров',
                choosePrinter: '-- Выберите принтер --',
                addManually: 'Добавить принтер вручную',
                printerName: 'Название принтера',
                printerNamePlaceholder: 'например, Bambu Lab X1',
                ipAddress: 'IP-адрес',
                ipPlaceholder: '192.168.1.100',
                printerCode: 'Код принтера',
                printerCodePlaceholder: 'Введите код доступа принтера',
                serialNumber: 'Серийный номер',
                serialPlaceholder: 'Введите серийный номер принтера',
                addPrinterBtn: 'Добавить принтер',
                configuredPrinters: 'Настроенные принтеры',
                deleteBtn: 'Удалить',
                settingsSaved: 'Настройки сохранены!',
                weatherSettingsSaved: 'Настройки погоды сохранены!',
                locationAdded: 'Местоположение добавлено!',
                printerAdded: 'Принтер добавлен!',
                error: 'Ошибка',
                errorLoading: 'Ошибка загрузки',
                scanning: 'Сканирование',
                starting: 'Запуск',
                noDiscoveredPrinters: 'Принтеры не найдены',
                discoveryComplete: 'Поиск завершен',
                a1MiniSerialWarning: '⚠️ A1 Mini ТРЕБУЕТ серийный номер. Найдите его в приложении Bambu → Устройство → Настройки → Информация об устройстве'
            }
        };
        
        let currentLang = 'en';
        
        function t(key) {
            return translations[currentLang][key] || translations['en'][key] || key;
        }
        
        function setLanguage(lang) {
            currentLang = lang;
            updatePageText();
        }
        
        function updatePageText() {
            // Update all translatable elements
            document.title = t('title');
            document.querySelector('h1').textContent = '🎨 ' + t('title');
            // We'll add data-i18n attributes to elements for automatic translation
        }
        
        function translatePage() {
            document.title = t('title');
            
            // Update page heading
            const h1 = document.querySelector('h1');
            if (h1) h1.textContent = '🎨 ' + t('title');
            
            // Translate all elements with data-i18n attribute
            document.querySelectorAll('[data-i18n]').forEach(elem => {
                const key = elem.getAttribute('data-i18n');
                if (elem.tagName === 'INPUT' && elem.type !== 'button') {
                    elem.placeholder = t(key);
                } else {
                    elem.textContent = t(key);
                }
            });
            
            // Update select options if needed
            updateSelectOptions();
        }
        
        function updateSelectOptions() {
            // Theme select
            const themeSelect = document.getElementById('theme');
            if (themeSelect) {
                const darkOpt = themeSelect.querySelector('option[value="dark"]');
                const lightOpt = themeSelect.querySelector('option[value="light"]');
                if (darkOpt) darkOpt.textContent = t('themeDark');
                if (lightOpt) lightOpt.textContent = t('themeLight');
            }
            
            // Timezone select first option
            const tzSelect = document.getElementById('timezone');
            if (tzSelect && tzSelect.options[0]) {
                tzSelect.options[0].textContent = t('selectTimezone');
            }
            
            // Discovered printer select
            const printerSelect = document.getElementById('discoveredPrinterDropdown');
            if (printerSelect && printerSelect.options[0]) {
                printerSelect.options[0].textContent = t('choosePrinter');
            }
        }
        
        // Use relative URLs so fetch works from any hostname/IP
        const apiBase = '';
        
        // Brightness slider
        document.getElementById('brightness').addEventListener('input', function() {
            document.getElementById('brightnessVal').textContent = this.value;
        });
        
        function showStatus(elementId, message, isSuccess) {
            const elem = document.getElementById(elementId);
            // Only update if text actually changed to avoid DOM thrashing
            if (elem.textContent !== message) {
                elem.textContent = message;
            }
            elem.className = 'status ' + (isSuccess ? 'success' : 'error');
            
            // Only set auto-clear timeout for non-transient messages (don't clear during discovery)
            // Check if message contains scanning/starting keywords in any language
            const isTransient = message.includes(t('scanning')) || message.includes(t('starting'));
            if (!isTransient) {
                // Clear after 5 seconds for persistent messages (errors, success)
                if (!elem.dataset.clearTimeout) {
                    elem.dataset.clearTimeout = setTimeout(() => { 
                        elem.textContent = ''; 
                        delete elem.dataset.clearTimeout;
                    }, 5000);
                }
            } else {
                // Cancel any pending clear for transient messages
                if (elem.dataset.clearTimeout) {
                    clearTimeout(parseInt(elem.dataset.clearTimeout));
                    delete elem.dataset.clearTimeout;
                }
            }
        }
        
        // Settings functions
        function saveSettings() {
            const data = {
                brightness: parseInt(document.getElementById('brightness').value),
                theme: document.getElementById('theme').value,
                timezone: document.getElementById('timezone').value,
                language: document.getElementById('language').value
            };
            
            fetch(apiBase + '/api/config', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(r => r.json())
            .then(d => showStatus('settingsStatus', t('settingsSaved'), d.success))
            .catch(e => showStatus('settingsStatus', 'Error: ' + e, false));
        }
        
        function loadSettings() {
            fetch(apiBase + '/api/config')
            .then(r => r.json())
            .then(d => {
                document.getElementById('brightness').value = d.brightness;
                document.getElementById('brightnessVal').textContent = d.brightness;
                document.getElementById('theme').value = d.theme;
                document.getElementById('timezone').value = d.timezone;
                document.getElementById('language').value = d.language || 'en';
            })
            .catch(e => showStatus('settingsStatus', t('errorLoading') + ': ' + e, false));
        }
        
        // Weather functions
        function saveWeatherSettings() {
            const data = {
                apiKey: document.getElementById('apiKey').value
            };
            
            fetch(apiBase + '/api/weather', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(r => r.json())
            .then(d => showStatus('weatherStatus', t('weatherSettingsSaved'), d.success))
            .catch(e => showStatus('weatherStatus', 'Error: ' + e, false));
        }
        
        function loadWeatherSettings() {
            fetch(apiBase + '/api/weather')
            .then(r => r.json())
            .then(d => {
                document.getElementById('apiKey').value = d.apiKey || '';
            })
            .catch(e => showStatus('weatherStatus', t('errorLoading') + ': ' + e, false));
        }
        
        // Weather locations functions
        function addWeatherLocation() {
            const data = {
                name: document.getElementById('locationName').value,
                city: document.getElementById('locationCity').value,
                country: document.getElementById('locationCountry').value,
                latitude: parseFloat(document.getElementById('locationLat').value),
                longitude: parseFloat(document.getElementById('locationLon').value)
            };
            
            if (!data.name || !data.city || !data.country || isNaN(data.latitude) || isNaN(data.longitude)) {
                showStatus('locationStatus', 'Please fill all fields with valid values', false);
                return;
            }
            
            fetch(apiBase + '/api/locations', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(r => r.json())
            .then(d => {
                showStatus('locationStatus', t('locationAdded'), d.success);
                if (d.success) {
                    document.getElementById('locationName').value = '';
                    document.getElementById('locationCity').value = '';
                    document.getElementById('locationCountry').value = '';
                    document.getElementById('locationLat').value = '';
                    document.getElementById('locationLon').value = '';
                    loadWeatherLocations();
                }
            })
            .catch(e => showStatus('locationStatus', 'Error: ' + e, false));
        }
        
        function loadWeatherLocations() {
            fetch(apiBase + '/api/locations')
            .then(r => r.json())
            .then(d => {
                const list = document.getElementById('locationList');
                list.innerHTML = '';
                if (d.locations && d.locations.length > 0) {
                    d.locations.forEach((loc, i) => {
                        const item = document.createElement('li');
                        item.className = 'printer-item';
                        item.innerHTML = `<strong>${loc.name}</strong> - ${loc.city}, ${loc.country} (${loc.latitude.toFixed(4)}°, ${loc.longitude.toFixed(4)}°) 
                                         <button onclick="removeWeatherLocation(${i})" style="float:right">❌</button>`;
                        list.appendChild(item);
                    });
                } else {
                    list.innerHTML = '<li class="printer-item">No locations configured</li>';
                }
            })
            .catch(e => showStatus('locationStatus', t('errorLoading') + ': ' + e, false));
        }
        
        function removeWeatherLocation(index) {
            fetch(apiBase + '/api/locations?index=' + index, { method: 'DELETE' })
            .then(r => r.json())
            .then(d => {
                showStatus('locationStatus', 'Location removed!', d.success);
                loadWeatherLocations();
            })
            .catch(e => showStatus('locationStatus', 'Error: ' + e, false));
        }
        
        // Printer functions
        let discoveredPrinters = [];
        
        let discoveryCheckInterval = null;
        
        function discoverPrinters() {
            // Clear any existing polling
            if (discoveryCheckInterval) {
                clearTimeout(discoveryCheckInterval);
                discoveryCheckInterval = null;
            }
            
            showStatus('discoverStatus', t('starting') + ' network scan...', true);
            document.getElementById('discoveredPrinterSection').style.display = 'none';
            
            // Start async discovery on server
            fetch(apiBase + '/api/printers/discover', { method: 'POST' })
            .then(r => r.json())
            .then(d => {
                if (d.status === 'started') {
                    showStatus('discoverStatus', t('scanning') + ' network for printers... 0%', true);
                    // Start polling for discovery status
                    discoveryCheckInterval = setTimeout(checkDiscoveryStatus, 300);
                } else {
                    showStatus('discoverStatus', 'Error starting discovery: ' + (d.error || 'unknown error'), false);
                }
            })
            .catch(e => {
                showStatus('discoverStatus', 'Discovery error: ' + e, false);
            });
        }
        
        function checkDiscoveryStatus() {
            // Clear the current interval first
            if (discoveryCheckInterval) {
                clearTimeout(discoveryCheckInterval);
                discoveryCheckInterval = null;
            }
            
            fetch(apiBase + '/api/printers/discover/status')
            .then(r => r.json())
            .then(d => {
                console.log('Discovery status:', d);
                const statusElem = document.getElementById('discoverStatus');
                
                if (d.in_progress) {
                    // Update status text directly without going through showStatus to avoid clearing
                    statusElem.textContent = `${t('scanning')}: ${d.progress}% complete (${d.count} found so far)...`;
                    statusElem.className = 'status success';
                    // Schedule next poll in 300ms
                    discoveryCheckInterval = setTimeout(checkDiscoveryStatus, 300);
                } else {
                    // Discovery complete - make sure interval is cleared
                    if (discoveryCheckInterval) {
                        clearTimeout(discoveryCheckInterval);
                        discoveryCheckInterval = null;
                    }
                    
                    if (d.discovered && d.discovered.length > 0) {
                        discoveredPrinters = d.discovered;
                        const dropdown = document.getElementById('discoveredPrinterDropdown');
                        dropdown.innerHTML = '<option value="">-- Choose a printer --</option>';
                        
                        d.discovered.forEach((p, idx) => {
                            const option = document.createElement('option');
                            option.value = idx;
                            option.textContent = `${p.hostname} (${p.model}) - ${p.ip_address}`;
                            dropdown.appendChild(option);
                        });
                        
                        // Found printers - show success message
                        const statusElem = document.getElementById('discoverStatus');
                        statusElem.textContent = `✓ Found ${d.discovered.length} printer(s)!`;
                        statusElem.className = 'status success';
                        document.getElementById('discoveredPrinterSection').style.display = 'block';
                    } else {
                        // No printers found - show message in error style but keep it displayed
                        const statusElem = document.getElementById('discoverStatus');
                        statusElem.textContent = 'No printers found';
                        statusElem.className = 'status error';  // Use error styling (red)
                        document.getElementById('discoveredPrinterSection').style.display = 'none';
                    }
                }
            })
            .catch(e => {
                showStatus('discoverStatus', 'Status check error: ' + e, false);
                if (discoveryCheckInterval) {
                    clearTimeout(discoveryCheckInterval);
                    discoveryCheckInterval = null;
                }
            });
        }
        
        function selectDiscoveredPrinter() {
            const dropdown = document.getElementById('discoveredPrinterDropdown');
            const idx = parseInt(dropdown.value);
            
            if (!isNaN(idx) && idx >= 0 && idx < discoveredPrinters.length) {
                const printer = discoveredPrinters[idx];
                document.getElementById('printerName').value = printer.hostname;
                document.getElementById('printerIP').value = printer.ip_address;
                document.getElementById('printerSerial').value = '';
                validatePrinterForm(); // Update button states
                showStatus('printerStatus', 'Printer selected. Add code and click Fetch Serial.', true);
            }
        }
        
        function validatePrinterForm() {
            const name = document.getElementById('printerName').value.trim();
            const ip = document.getElementById('printerIP').value.trim();
            const token = document.getElementById('printerToken').value.trim();
            const serial = document.getElementById('printerSerial').value.trim();
            
            // Enable Fetch Serial button if IP and token are filled
            const fetchBtn = document.getElementById('fetchSerialBtn');
            fetchBtn.disabled = !(ip && token);
            
            // Enable Add Printer button if name, IP, and token are filled (serial is optional)
            const addBtn = document.getElementById('addPrinterBtn');
            addBtn.disabled = !(name && ip && token);
            
            // Update status message
            if (name && ip && token && !serial) {
                showStatus('printerStatus', 'Serial number optional - you can add it later or fetch it', true);
            }
        }
        
        function fetchPrinterSerial() {
            const ip = document.getElementById('printerIP').value;
            const token = document.getElementById('printerToken').value;
            
            if (!ip || !token) {
                showStatus('printerStatus', 'IP and Access Code required', false);
                return;
            }
            
            const statusElem = document.getElementById('printerStatus');
            statusElem.textContent = '📡 Querying printer via MQTT (up to 15s)...';
            statusElem.className = 'status success';
            document.getElementById('fetchSerialBtn').disabled = true;
            
            // Call our ESP32 backend to query the printer via MQTT
            fetch(apiBase + `/api/printer/query?ip=${encodeURIComponent(ip)}&code=${encodeURIComponent(token)}`, {
                signal: AbortSignal.timeout(20000)  // 20 second fetch timeout
            })
            .then(r => r.json())
            .then(d => {
                if (d.success && d.serial) {
                    document.getElementById('printerSerial').value = d.serial;
                    statusElem.textContent = '✓ Serial fetched: ' + d.serial;
                    statusElem.className = 'status success';
                    validatePrinterForm();
                } else {
                    statusElem.innerHTML = '✗ ' + (d.error || 'Could not fetch serial') + '<br>' +
                        '<small>Find serial: Printer Display → Settings → Network → Device Info</small>';
                    statusElem.className = 'status error';
                }
                document.getElementById('fetchSerialBtn').disabled = false;
            })
            .catch(e => {
                statusElem.innerHTML = '✗ Query timed out<br>' +
                    '<small>Find serial: Printer Display → Settings → Network → Device Info</small>';
                statusElem.className = 'status error';
                document.getElementById('fetchSerialBtn').disabled = false;
            });
        }
        
        function addPrinter() {
            const data = {
                name: document.getElementById('printerName').value,
                ip: document.getElementById('printerIP').value,
                token: document.getElementById('printerToken').value,
                serial: document.getElementById('printerSerial').value,
                disable_ssl_verify: document.getElementById('disableSslVerify').checked
            };
            
            if (!data.name || !data.ip || !data.token) {
                showStatus('printerStatus', 'Please fill all fields', false);
                return;
            }
            
            fetch(apiBase + '/api/printers', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(data)
            })
            .then(r => r.json())
            .then(d => {
                showStatus('printerStatus', t('printerAdded'), d.success);
                if (d.success) {
                    document.getElementById('printerName').value = '';
                    document.getElementById('printerIP').value = '';
                    document.getElementById('printerToken').value = '';
                    document.getElementById('printerSerial').value = '';
                    validatePrinterForm(); // Disable buttons after clearing
                    loadPrinters();
                }
            })
            .catch(e => showStatus('printerStatus', 'Error: ' + e, false));
        }
        
        function loadPrinters() {
            fetch(apiBase + '/api/printers')
            .then(r => r.json())
            .then(d => {
                const list = document.getElementById('printerList');
                list.innerHTML = '';
                if (d.printers && d.printers.length > 0) {
                    d.printers.forEach((p, i) => {
                        const item = document.createElement('li');
                        item.className = 'printer-item';
                        const sslStatus = p.disable_ssl_verify ? '🔓' : '🔒';
                        const sslTooltip = p.disable_ssl_verify ? 'SSL verification disabled' : 'SSL verification enabled';
                        item.innerHTML = `<strong>${p.name}</strong> - ${p.ip} <span title="${sslTooltip}">${sslStatus}</span>
                                         <button onclick="removePrinter(${i})" style="float:right">❌</button>`;
                        list.appendChild(item);
                    });
                } else {
                    list.innerHTML = '<li class="printer-item">No printers configured</li>';
                }
            })
            .catch(e => showStatus('printerStatus', t('errorLoading') + ': ' + e, false));
        }
        
        function removePrinter(index) {
            fetch(apiBase + '/api/printers?index=' + index, { method: 'DELETE' })
            .then(r => r.json())
            .then(d => {
                showStatus('printerStatus', 'Printer removed!', d.success);
                loadPrinters();
            })
            .catch(e => showStatus('printerStatus', 'Error: ' + e, false));
        }
        
        function loadDeviceInfo() {
            fetch(apiBase + '/api/device-info')
            .then(r => r.json())
            .then(d => {
                let infoText = `Device: ${d.device_name}\n`;
                infoText += `Version: ${d.version}\n`;
                infoText += `Free Heap: ${(d.free_heap / 1024).toFixed(1)} KB\n`;
                infoText += `Min Free Heap: ${(d.min_free_heap / 1024).toFixed(1)} KB\n`;
                if (d.ssid) infoText += `SSID: ${d.ssid}\n`;
                if (d.rssi) infoText += `Signal: ${d.rssi} dBm\n`;
                if (d.ip_address) infoText += `IP Address: ${d.ip_address}\n`;
                document.getElementById('deviceInfo').textContent = infoText;
            })
            .catch(e => document.getElementById('deviceInfo').textContent = 'Error: ' + e);
        }
        
        // Network functions
        function loadNetworks() {
            fetch(apiBase + '/api/networks')
            .then(r => r.json())
            .then(d => {
                const list = document.getElementById('networkList');
                list.innerHTML = '';
                if (d.networks && d.networks.length > 0) {
                    d.networks.forEach((n, i) => {
                        const item = document.createElement('li');
                        item.className = 'printer-item';
                        item.innerHTML = `<strong>${n.name}</strong> - ${n.subnet} 
                                         <button onclick="removeNetwork(${i})" style="float:right">❌</button>`;
                        list.appendChild(item);
                    });
                } else {
                    list.innerHTML = '<li class="printer-item">No networks configured</li>';
                }
            })
            .catch(e => showStatus('networkStatus', t('errorLoading') + ': ' + e, false));
        }
        
        function addNetwork() {
            const name = document.getElementById('networkName').value.trim();
            const subnet = document.getElementById('networkSubnet').value.trim();
            
            if (!name || !subnet) {
                showStatus('networkStatus', 'Please fill all fields', false);
                return;
            }
            
            // Basic CIDR validation
            if (!subnet.match(/^\d+\.\d+\.\d+\.\d+\/\d+$/)) {
                showStatus('networkStatus', 'Invalid CIDR format. Use: 192.168.1.0/24', false);
                return;
            }
            
            fetch(apiBase + '/api/networks', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name, subnet })
            })
            .then(r => r.json())
            .then(d => {
                showStatus('networkStatus', 'Network added!', d.success);
                if (d.success) {
                    document.getElementById('networkName').value = '';
                    document.getElementById('networkSubnet').value = '';
                    loadNetworks();
                }
            })
            .catch(e => showStatus('networkStatus', 'Error: ' + e, false));
        }
        
        function removeNetwork(index) {
            fetch(apiBase + '/api/networks?index=' + index, { method: 'DELETE' })
            .then(r => r.json())
            .then(d => {
                showStatus('networkStatus', 'Network removed!', d.success);
                loadNetworks();
            })
            .catch(e => showStatus('networkStatus', 'Error: ' + e, false));
        }
        
        // Load data on page load
        window.addEventListener('load', () => {
            // Load language from localStorage or default to English
            const savedLang = localStorage.getItem('language') || 'en';
            currentLang = savedLang;
            document.getElementById('language').value = savedLang;
            
            // Add language change listener
            document.getElementById('language').addEventListener('change', (e) => {
                currentLang = e.target.value;
                localStorage.setItem('language', currentLang);
                translatePage();
            });
            
            translatePage();
            loadSettings();
            loadWeatherSettings();
            loadWeatherLocations();
            loadPrinters();
            loadNetworks();
            loadDeviceInfo();
        });
    </script>
</body>
</html>
)rawliteral";

// Root handler
esp_err_t WebServer::handle_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "0");
    return httpd_resp_send(req, HTML_PAGE, strlen(HTML_PAGE));
}

// Config GET
esp_err_t WebServer::handle_api_config_get(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "brightness", cfg->Brightness);
    cJSON_AddStringToObject(root, "theme", cfg->CurrentTheme.c_str());
    cJSON_AddStringToObject(root, "timezone", cfg->TimeZone.c_str());
    cJSON_AddStringToObject(root, "language", cfg->Language.c_str());
    
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Config POST
esp_err_t WebServer::handle_api_config_post(httpd_req_t *req) {
    char content[256] = {0};
    int recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (recv_len <= 0) {
        return httpd_resp_send_500(req);
    }
    
    cJSON *data = cJSON_Parse(content);
    if (!data) {
        return httpd_resp_send_500(req);
    }
    
    if (cJSON_HasObjectItem(data, "brightness")) {
        cfg->Brightness = cJSON_GetObjectItem(data, "brightness")->valueint;
    }
    if (cJSON_HasObjectItem(data, "theme")) {
        cfg->CurrentTheme = cJSON_GetObjectItem(data, "theme")->valuestring;
    }
    if (cJSON_HasObjectItem(data, "timezone")) {
        cfg->TimeZone = cJSON_GetObjectItem(data, "timezone")->valuestring;
    }
    if (cJSON_HasObjectItem(data, "language")) {
        cfg->Language = cJSON_GetObjectItem(data, "language")->valuestring;
    }
    
    cfg->save_config();
    
    // Notify GUI that config changed - update language and rebuild UI
    esp_event_post(TUX_EVENTS, TUX_EVENT_CONFIG_CHANGED, NULL, 0, pdMS_TO_TICKS(100));
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    char *json_str = cJSON_Print(response);
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(response);
    cJSON_Delete(data);
    
    ESP_LOGI(TAG, "Config updated via web");
    return err;
}

// Weather GET
esp_err_t WebServer::handle_api_weather_get(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "location", cfg->WeatherLocation.c_str());
    cJSON_AddStringToObject(root, "apiKey", cfg->WeatherAPIkey.c_str());
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Weather POST
esp_err_t WebServer::handle_api_weather_post(httpd_req_t *req) {
    char content[512] = {0};
    int recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (recv_len <= 0) {
        return httpd_resp_send_500(req);
    }
    
    cJSON *data = cJSON_Parse(content);
    if (!data) {
        return httpd_resp_send_500(req);
    }
    
    if (cJSON_HasObjectItem(data, "location")) {
        cfg->WeatherLocation = cJSON_GetObjectItem(data, "location")->valuestring;
    }
    if (cJSON_HasObjectItem(data, "apiKey")) {
        cfg->WeatherAPIkey = cJSON_GetObjectItem(data, "apiKey")->valuestring;
    }
    
    cfg->save_config();
    
    // Notify GUI that config changed - rebuild carousel (API key affects weather display)
    esp_event_post(TUX_EVENTS, TUX_EVENT_CONFIG_CHANGED, NULL, 0, pdMS_TO_TICKS(100));
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    char *json_str = cJSON_Print(response);
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(response);
    cJSON_Delete(data);
    
    ESP_LOGI(TAG, "Weather settings updated via web");
    return err;
}

// Locations GET
esp_err_t WebServer::handle_api_locations_get(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *locations_array = cJSON_CreateArray();
    
    if (cfg) {
        for (int i = 0; i < cfg->get_weather_location_count(); i++) {
            weather_location_t loc = cfg->get_weather_location(i);
            cJSON *loc_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(loc_obj, "name", loc.name.c_str());
            cJSON_AddStringToObject(loc_obj, "city", loc.city.c_str());
            cJSON_AddStringToObject(loc_obj, "country", loc.country.c_str());
            cJSON_AddNumberToObject(loc_obj, "latitude", loc.latitude);
            cJSON_AddNumberToObject(loc_obj, "longitude", loc.longitude);
            cJSON_AddItemToArray(locations_array, loc_obj);
        }
    }
    
    cJSON_AddItemToObject(root, "locations", locations_array);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Locations POST
esp_err_t WebServer::handle_api_locations_post(httpd_req_t *req) {
    char content[512] = {0};
    int recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (recv_len <= 0) {
        return httpd_resp_send_500(req);
    }
    
    cJSON *data = cJSON_Parse(content);
    if (!data) {
        return httpd_resp_send_500(req);
    }
    
    const char *name = cJSON_GetObjectItem(data, "name")->valuestring;
    const char *city = cJSON_GetObjectItem(data, "city")->valuestring;
    const char *country = cJSON_GetObjectItem(data, "country")->valuestring;
    cJSON *lat_obj = cJSON_GetObjectItem(data, "latitude");
    cJSON *lon_obj = cJSON_GetObjectItem(data, "longitude");
    
    if (name && city && country && lat_obj && lon_obj) {
        float latitude = lat_obj->valuedouble;
        float longitude = lon_obj->valuedouble;
        
        if (cfg) {
            cfg->add_weather_location(name, city, country, latitude, longitude);
            cfg->save_config();
            
            // Notify GUI that config changed - rebuild carousel
            esp_event_post(TUX_EVENTS, TUX_EVENT_CONFIG_CHANGED, NULL, 0, pdMS_TO_TICKS(100));
            
            ESP_LOGI(TAG, "Location added: %s (%s, %s) at %.4f, %.4f", 
                     name, city, country, latitude, longitude);
            
            cJSON *response = cJSON_CreateObject();
            cJSON_AddBoolToObject(response, "success", true);
            char *json_str = cJSON_Print(response);
            
            httpd_resp_set_type(req, "application/json");
            esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
            
            free(json_str);
            cJSON_Delete(response);
        } else {
            return httpd_resp_send_500(req);
        }
    } else {
        return httpd_resp_send_500(req);
    }
    
    cJSON_Delete(data);
    return ESP_OK;
}

// Locations DELETE
esp_err_t WebServer::handle_api_locations_delete(httpd_req_t *req) {
    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char index_str[8] = {0};
        if (httpd_query_key_value(query, "index", index_str, sizeof(index_str)) == ESP_OK) {
            int index = atoi(index_str);
            if (cfg) {
                cfg->remove_weather_location(index);
                cfg->save_config();
                
                // Notify GUI that config changed - rebuild carousel
                esp_event_post(TUX_EVENTS, TUX_EVENT_CONFIG_CHANGED, NULL, 0, pdMS_TO_TICKS(100));
                
                ESP_LOGI(TAG, "Location removed: index %d", index);
                
                cJSON *response = cJSON_CreateObject();
                cJSON_AddBoolToObject(response, "success", true);
                char *json_str = cJSON_Print(response);
                
                httpd_resp_set_type(req, "application/json");
                esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
                
                free(json_str);
                cJSON_Delete(response);
                return err;
            }
        }
    }
    return httpd_resp_send_500(req);
}

// Printers GET
esp_err_t WebServer::handle_api_printers_get(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *printers_array = cJSON_CreateArray();
    
    for (int i = 0; i < cfg->get_printer_count(); i++) {
        printer_config_t printer = cfg->get_printer(i);
        cJSON *printer_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(printer_obj, "name", printer.name.c_str());
        cJSON_AddStringToObject(printer_obj, "ip", printer.ip_address.c_str());
        cJSON_AddStringToObject(printer_obj, "serial", printer.serial.c_str());
        cJSON_AddBoolToObject(printer_obj, "disable_ssl_verify", printer.disable_ssl_verify);
        cJSON_AddItemToArray(printers_array, printer_obj);
    }
    
    cJSON_AddItemToObject(root, "printers", printers_array);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Printers POST
esp_err_t WebServer::handle_api_printers_post(httpd_req_t *req) {
    char content[512] = {0};
    int recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (recv_len <= 0) {
        return httpd_resp_send_500(req);
    }
    
    cJSON *data = cJSON_Parse(content);
    if (!data) {
        return httpd_resp_send_500(req);
    }
    
    const char *name = cJSON_GetObjectItem(data, "name")->valuestring;
    const char *ip = cJSON_GetObjectItem(data, "ip")->valuestring;
    const char *token = cJSON_GetObjectItem(data, "token")->valuestring;
    const char *serial = cJSON_GetObjectItem(data, "serial") ? 
                        cJSON_GetObjectItem(data, "serial")->valuestring : "";
    
    // Get disable_ssl_verify (defaults to true for easier setup)
    cJSON *ssl_verify_item = cJSON_GetObjectItem(data, "disable_ssl_verify");
    bool disable_ssl_verify = ssl_verify_item ? cJSON_IsTrue(ssl_verify_item) : true;
    
    if (name && ip && token) {
        cfg->add_printer(name, ip, token, serial ? serial : "");
        
        // Update the last added printer's SSL setting
        if (cfg->get_printer_count() > 0) {
            int last_index = cfg->get_printer_count() - 1;
            printer_config_t printer = cfg->get_printer(last_index);
            printer.disable_ssl_verify = disable_ssl_verify;
            cfg->PrinterList[last_index] = printer;
        }
        
        cfg->save_config();
        
        // Notify GUI that config changed - rebuild carousel
        esp_event_post(TUX_EVENTS, TUX_EVENT_CONFIG_CHANGED, NULL, 0, pdMS_TO_TICKS(100));
        
        ESP_LOGI(TAG, "Printer added: %s at %s (serial: %s, SSL verify: %s)", 
                 name, ip, serial ? serial : "", disable_ssl_verify ? "disabled" : "enabled");
        
        // Reinitialize BambuMonitor with the new printer configuration
        extern esp_err_t reinit_bambu_monitor(void);
        if (reinit_bambu_monitor() == ESP_OK) {
            ESP_LOGI(TAG, "BambuMonitor reinitialized with new printer");
        } else {
            ESP_LOGW(TAG, "Failed to reinitialize BambuMonitor - reboot may be required");
        }
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", true);
        char *json_str = cJSON_Print(response);
        
        httpd_resp_set_type(req, "application/json");
        esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
        
        free(json_str);
        cJSON_Delete(response);
    } else {
        return httpd_resp_send_500(req);
    }
    
    cJSON_Delete(data);
    return ESP_OK;
}

// Printers DELETE
esp_err_t WebServer::handle_api_printers_delete(httpd_req_t *req) {
    char query[256] = {0};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char index_str[8] = {0};
        if (httpd_query_key_value(query, "index", index_str, sizeof(index_str)) == ESP_OK) {
            int index = atoi(index_str);
            if (cfg) {
                cfg->remove_printer(index);
                cfg->save_config();
                
                // Notify GUI that config changed - rebuild carousel
                esp_event_post(TUX_EVENTS, TUX_EVENT_CONFIG_CHANGED, NULL, 0, pdMS_TO_TICKS(100));
                
                ESP_LOGI(TAG, "Printer removed: index %d", index);
                
                cJSON *response = cJSON_CreateObject();
                cJSON_AddBoolToObject(response, "success", true);
                char *json_str = cJSON_Print(response);
                
                httpd_resp_set_type(req, "application/json");
                esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
                
                free(json_str);
                cJSON_Delete(response);
                return err;
            }
        }
    }
    return httpd_resp_send_500(req);
}

// Printers DISCOVER (Auto-detect Bambu printers via mDNS)
esp_err_t WebServer::handle_api_printers_discover(httpd_req_t *req) {
    // Check request method
    if (req->method == HTTP_POST) {
        // Start async discovery task
        if (g_discovery_in_progress) {
            // Already discovering
            cJSON *response = cJSON_CreateObject();
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "status", "already_running");
            char *json_str = cJSON_Print(response);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, json_str, strlen(json_str));
            free(json_str);
            cJSON_Delete(response);
            return ESP_OK;
        }
        
        // Kill previous task if any
        if (g_discovery_task != nullptr) {
            vTaskDelete(g_discovery_task);
            g_discovery_task = nullptr;
        }
        
        // Start new discovery task (4KB stack, priority 1)
        xTaskCreate(discovery_task_handler, "discovery_task", 8192, NULL, 1, &g_discovery_task);
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "status", "started");
        char *json_str = cJSON_Print(response);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_str, strlen(json_str));
        free(json_str);
        cJSON_Delete(response);
        return ESP_OK;
    }
    
    // GET request - return last results (deprecated, use /status instead)
    cJSON *root = cJSON_CreateObject();
    cJSON *printers_array = cJSON_CreateArray();
    
    for (const auto &ip : g_discovered_ips) {
        cJSON *printer_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(printer_obj, "hostname", "Bambu Lab Printer");
        cJSON_AddStringToObject(printer_obj, "ip_address", ip.c_str());
        cJSON_AddStringToObject(printer_obj, "model", "Unknown");
        cJSON_AddItemToArray(printers_array, printer_obj);
    }
    
    cJSON_AddItemToObject(root, "discovered", printers_array);
    cJSON_AddNumberToObject(root, "count", (int)g_discovered_ips.size());
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Printers Discovery Status GET
esp_err_t WebServer::handle_api_printers_discover_status(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "in_progress", g_discovery_in_progress);
    cJSON_AddNumberToObject(root, "progress", g_discovery_progress);
    
    // Add discovered printers with mutex protection
    cJSON *printers_array = cJSON_CreateArray();
    int count = 0;
    
    if (g_discovered_ips_mutex && xSemaphoreTake(g_discovered_ips_mutex, pdMS_TO_TICKS(100))) {
        for (const auto &ip : g_discovered_ips) {
            cJSON *printer_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(printer_obj, "hostname", "Bambu Lab Printer");
            cJSON_AddStringToObject(printer_obj, "ip_address", ip.c_str());
            cJSON_AddStringToObject(printer_obj, "model", "Unknown");
            cJSON_AddItemToArray(printers_array, printer_obj);
        }
        count = g_discovered_ips.size();
        xSemaphoreGive(g_discovered_ips_mutex);
    }
    
    cJSON_AddItemToObject(root, "discovered", printers_array);
    cJSON_AddNumberToObject(root, "count", count);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Get printer info (query printer for serial via MQTT topic)
// Can accept either IP+token OR topic path for serial extraction
// Usage: /api/printer/info?ip=10.13.13.85&token=5d35821c
//   or: /api/printer/info?topic=device/00M09D530200738/report
esp_err_t WebServer::handle_api_printer_info(httpd_req_t *req) {
    char query[512] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    cJSON *root = cJSON_CreateObject();
    
    // Check if user provided a topic (for manual MQTT topic extraction)
    char topic_str[256] = {0};
    if (httpd_query_key_value(query, "topic", topic_str, sizeof(topic_str)) == ESP_OK && strlen(topic_str) > 0) {
        std::string serial = PrinterDiscovery::extract_serial_from_topic(topic_str);
        if (!serial.empty()) {
            cJSON_AddStringToObject(root, "serial", serial.c_str());
            cJSON_AddStringToObject(root, "status", "✓ Serial extracted from MQTT topic");
            cJSON_AddStringToObject(root, "topic", topic_str);
        } else {
            cJSON_AddStringToObject(root, "status", "✗ Invalid topic format");
            cJSON_AddStringToObject(root, "expected_format", "device/SERIAL_NUMBER/report");
        }
    } else {
        // Check for IP and token (for testing connection)
        char ip_str[16] = {0};
        char token_str[32] = {0};
        
        if (httpd_query_key_value(query, "ip", ip_str, sizeof(ip_str)) == ESP_OK && strlen(ip_str) > 0) {
            cJSON_AddStringToObject(root, "ip", ip_str);
            
            // Test connection to printer
            bool connected = PrinterDiscovery::test_connection(ip_str, 8883, 500);
            if (connected) {
                cJSON_AddStringToObject(root, "connection", "✓ Port 8883 responding");
                cJSON_AddStringToObject(root, "data_available", 
                    "Printer MQTT publishes extensive status data including:\n"
                    "- Serial number (from device/{SERIAL}/report topic)\n"
                    "- Printer state (IDLE, RUNNING, PAUSE, etc.)\n"
                    "- Temperature sensors (bed, nozzle, chamber)\n"
                    "- AMS (filament) status\n"
                    "- Print progress and error codes\n"
                    "- WiFi signal strength\n"
                    "- Fan states and gear settings\n"
                    "- Model ID and build plate info\n"
                    "- 80+ additional monitoring fields");
                cJSON_AddStringToObject(root, "next_step", 
                    "To retrieve printer serial:\n"
                    "1. Use MQTT Explorer:\n"
                    "   - Connect: Host=<ip>, Port=8883, Username=bblp, Password=<access_code>, TLS=enabled\n"
                    "   - Find topic: device/{SERIAL_NUMBER}/report\n"
                    "   - Extract SERIAL_NUMBER\n"
                    "2. Or call: /api/printer/info?topic=device/{YOUR_SERIAL}/report\n"
                    "   (will validate and extract)");
            } else {
                cJSON_AddStringToObject(root, "connection", "✗ Port 8883 not responding");
                cJSON_AddStringToObject(root, "error", "Printer may be offline or unreachable");
            }
        } else {
            cJSON_AddStringToObject(root, "error", "Missing parameters");
            cJSON_AddStringToObject(root, "usage_1", "/api/printer/info?ip=10.13.13.85&token=5d35821c");
            cJSON_AddStringToObject(root, "usage_2", "/api/printer/info?topic=device/00M09D530200738/report");
        }
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Query Printer Status GET - connect via MQTT and retrieve printer telemetry
// Usage: /api/printer/query?ip=10.13.13.85&code=5d35821c
esp_err_t WebServer::handle_api_printer_query(httpd_req_t *req) {
    char query[512] = {0};
    httpd_req_get_url_query_str(req, query, sizeof(query));
    
    char ip_str[16] = {0};
    char code_str[32] = {0};
    
    if (httpd_query_key_value(query, "ip", ip_str, sizeof(ip_str)) != ESP_OK || strlen(ip_str) == 0) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "Missing 'ip' parameter");
        cJSON_AddStringToObject(root, "usage", "/api/printer/query?ip=10.13.13.85&code=5d35821c");
        
        char *json_str = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_str, strlen(json_str));
        
        free(json_str);
        cJSON_Delete(root);
        return ESP_OK;
    }
    
    if (httpd_query_key_value(query, "code", code_str, sizeof(code_str)) != ESP_OK || strlen(code_str) == 0) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "Missing 'code' parameter");
        cJSON_AddStringToObject(root, "usage", "/api/printer/query?ip=10.13.13.85&code=5d35821c");
        
        char *json_str = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_str, strlen(json_str));
        
        free(json_str);
        cJSON_Delete(root);
        return ESP_OK;
    }
    
    // Query printer status via MQTT - 10 second timeout to wait for periodic report
    PrinterDiscovery::PrinterStatus status = PrinterDiscovery::query_printer_status(ip_str, code_str, 10000);
    
    cJSON *root = cJSON_CreateObject();
    
    if (!status.serial.empty()) {
        cJSON_AddBoolToObject(root, "success", true);
        cJSON_AddStringToObject(root, "serial", status.serial.c_str());
        cJSON_AddStringToObject(root, "ip", status.ip_address.c_str());
        
        if (!status.state.empty()) cJSON_AddStringToObject(root, "state", status.state.c_str());
        
        cJSON *temps = cJSON_CreateObject();
        cJSON_AddNumberToObject(temps, "bed_current", status.bed_temperature);
        cJSON_AddNumberToObject(temps, "bed_target", status.bed_target_temperature);
        cJSON_AddNumberToObject(temps, "nozzle_current", status.nozzle_temperature);
        cJSON_AddNumberToObject(temps, "nozzle_target", status.nozzle_target_temperature);
        cJSON_AddItemToObject(root, "temperatures", temps);
        
        if (status.ams_status >= 0) cJSON_AddNumberToObject(root, "ams_status", status.ams_status);
        if (status.ams_rfid_status >= 0) cJSON_AddNumberToObject(root, "ams_rfid_status", status.ams_rfid_status);
        if (!status.wifi_signal.empty()) cJSON_AddStringToObject(root, "wifi_signal", status.wifi_signal.c_str());
        if (status.print_error >= 0) cJSON_AddNumberToObject(root, "print_error", status.print_error);
        if (!status.model_id.empty()) cJSON_AddStringToObject(root, "model_id", status.model_id.c_str());
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "Failed to retrieve printer status via MQTT");
        cJSON_AddStringToObject(root, "ip", ip_str);
        cJSON_AddStringToObject(root, "hint", "Ensure access code is correct and printer is online");
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Test Connection GET - for testing single IP/port connectivity
// Usage: /api/test/connection?ip=10.13.13.1&port=8883
esp_err_t WebServer::handle_api_test_connection(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    char buf[256];
    size_t buf_len = 0;
    
    // Extract query string
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            // Parse IP parameter
            char ip_str[16] = {0};
            char port_str[6] = {0};
            
            if (httpd_query_key_value(buf, "ip", ip_str, sizeof(ip_str)) != ESP_OK) {
                cJSON_AddBoolToObject(root, "success", false);
                cJSON_AddStringToObject(root, "error", "Missing 'ip' parameter");
                char *json_str = cJSON_Print(root);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, json_str, strlen(json_str));
                free(json_str);
                cJSON_Delete(root);
                return ESP_OK;
            }
            
            if (httpd_query_key_value(buf, "port", port_str, sizeof(port_str)) != ESP_OK) {
                cJSON_AddBoolToObject(root, "success", false);
                cJSON_AddStringToObject(root, "error", "Missing 'port' parameter");
                char *json_str = cJSON_Print(root);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, json_str, strlen(json_str));
                free(json_str);
                cJSON_Delete(root);
                return ESP_OK;
            }
            
            int port = atoi(port_str);
            if (port <= 0 || port > 65535) {
                cJSON_AddBoolToObject(root, "success", false);
                cJSON_AddStringToObject(root, "error", "Invalid port number");
                char *json_str = cJSON_Print(root);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, json_str, strlen(json_str));
                free(json_str);
                cJSON_Delete(root);
                return ESP_OK;
            }
            
            ESP_LOGI(TAG, "[Test Connection] Testing %s:%d", ip_str, port);
            
            // Create socket for testing
            int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (sock < 0) {
                ESP_LOGE(TAG, "[Test Connection] Failed to create socket");
                cJSON_AddBoolToObject(root, "success", false);
                cJSON_AddStringToObject(root, "error", "Failed to create socket");
            } else {
                // Set connection timeout to 500ms (same as discovery)
                struct timeval tv;
                tv.tv_sec = 0;
                tv.tv_usec = 500000;  // 500ms
                setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
                
                // Set socket to non-blocking for faster timeout
                int flags = fcntl(sock, F_GETFL, 0);
                fcntl(sock, F_SETFL, flags | O_NONBLOCK);
                
                // Create address structure
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_port = htons(port);
                addr.sin_addr.s_addr = inet_addr(ip_str);
                
                // Try to connect
                int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
                
                if (result == 0) {
                    // Connected successfully
                    cJSON_AddBoolToObject(root, "success", true);
                    cJSON_AddStringToObject(root, "message", "Connection successful");
                    ESP_LOGI(TAG, "[Test Connection] ✓ Connected to %s:%d", ip_str, port);
                } else {
                    int err = errno;
                    if (err == EINPROGRESS || err == EWOULDBLOCK) {
                        // Connection in progress (expected for non-blocking), wait a bit
                        fd_set writefds;
                        FD_ZERO(&writefds);
                        FD_SET(sock, &writefds);
                        
                        struct timeval timeout;
                        timeout.tv_sec = 0;
                        timeout.tv_usec = 500000;  // 500ms timeout
                        
                        int select_result = select(sock + 1, NULL, &writefds, NULL, &timeout);
                        
                        if (select_result > 0 && FD_ISSET(sock, &writefds)) {
                            // Check if connection succeeded
                            int so_error = 0;
                            socklen_t len = sizeof(so_error);
                            getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
                            
                            if (so_error == 0) {
                                cJSON_AddBoolToObject(root, "success", true);
                                cJSON_AddStringToObject(root, "message", "Connection successful");
                                ESP_LOGI(TAG, "[Test Connection] ✓ Connected to %s:%d", ip_str, port);
                            } else {
                                cJSON_AddBoolToObject(root, "success", false);
                                cJSON_AddStringToObject(root, "error", "Connection refused or timeout");
                                ESP_LOGI(TAG, "[Test Connection] ✗ Failed: %s:%d - %s", ip_str, port, strerror(so_error));
                            }
                        } else {
                            cJSON_AddBoolToObject(root, "success", false);
                            cJSON_AddStringToObject(root, "error", "Connection timeout (500ms)");
                            ESP_LOGI(TAG, "[Test Connection] ✗ Timeout: %s:%d", ip_str, port);
                        }
                    } else {
                        cJSON_AddBoolToObject(root, "success", false);
                        char error_msg[128];
                        snprintf(error_msg, sizeof(error_msg), "Connection failed: %s", strerror(err));
                        cJSON_AddStringToObject(root, "error", error_msg);
                        ESP_LOGI(TAG, "[Test Connection] ✗ Failed: %s:%d - %s", ip_str, port, strerror(err));
                    }
                }
                
                close(sock);
            }
            
            cJSON_AddStringToObject(root, "ip", ip_str);
            cJSON_AddNumberToObject(root, "port", port);
            
        } else {
            cJSON_AddBoolToObject(root, "success", false);
            cJSON_AddStringToObject(root, "error", "Failed to parse query string");
        }
    } else {
        cJSON_AddBoolToObject(root, "success", false);
        cJSON_AddStringToObject(root, "error", "Missing query parameters (use ?ip=X.X.X.X&port=XXXX)");
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Device Info GET
esp_err_t WebServer::handle_api_device_info(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    
    // Get app version from running partition
    const esp_app_desc_t *app_desc = esp_app_get_description();
    
    // Add device information
    cJSON_AddStringToObject(root, "device_name", cfg ? cfg->DeviceName.c_str() : "ESP32-TUX");
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "min_free_heap", esp_get_minimum_free_heap_size());
    cJSON_AddStringToObject(root, "version", app_desc->version);
    
    // Try to get WiFi info
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        cJSON_AddStringToObject(root, "ssid", (const char*)ap_info.ssid);
        cJSON_AddNumberToObject(root, "rssi", ap_info.rssi);
    }
    
    // Get IP address
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info) == ESP_OK) {
        char ip_str[16];
        snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
        cJSON_AddStringToObject(root, "ip_address", ip_str);
    }
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Networks GET
esp_err_t WebServer::handle_api_networks_get(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    cJSON *networks_array = cJSON_CreateArray();
    
    for (int i = 0; i < cfg->get_network_count(); i++) {
        network_config_t network = cfg->get_network(i);
        cJSON *network_obj = cJSON_CreateObject();
        cJSON_AddStringToObject(network_obj, "name", network.name.c_str());
        cJSON_AddStringToObject(network_obj, "subnet", network.subnet.c_str());
        cJSON_AddBoolToObject(network_obj, "enabled", network.enabled);
        cJSON_AddItemToArray(networks_array, network_obj);
    }
    
    cJSON_AddItemToObject(root, "networks", networks_array);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(root);
    return err;
}

// Networks POST
esp_err_t WebServer::handle_api_networks_post(httpd_req_t *req) {
    char content[256] = {0};
    int recv_len = httpd_req_recv(req, content, sizeof(content) - 1);
    
    if (recv_len <= 0) {
        return httpd_resp_send_500(req);
    }
    
    cJSON *data = cJSON_Parse(content);
    if (!data) {
        return httpd_resp_send_500(req);
    }
    
    const char *name = cJSON_GetObjectItem(data, "name") ? 
                      cJSON_GetObjectItem(data, "name")->valuestring : "";
    const char *subnet = cJSON_GetObjectItem(data, "subnet") ? 
                        cJSON_GetObjectItem(data, "subnet")->valuestring : "";
    
    if (name && subnet && strlen(name) > 0 && strlen(subnet) > 0) {
        cfg->add_network(name, subnet);
        cfg->save_config();
        
        ESP_LOGI(TAG, "Network added: %s (%s)", name, subnet);
        
        cJSON *response = cJSON_CreateObject();
        cJSON_AddBoolToObject(response, "success", true);
        char *json_str = cJSON_Print(response);
        
        httpd_resp_set_type(req, "application/json");
        esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
        
        free(json_str);
        cJSON_Delete(response);
    } else {
        return httpd_resp_send_500(req);
    }
    
    cJSON_Delete(data);
    return ESP_OK;
}

// Networks DELETE
esp_err_t WebServer::handle_api_networks_delete(httpd_req_t *req) {
    char query[64] = {0};
    size_t query_len = httpd_req_get_url_query_len(req);
    
    if (query_len > 0 && query_len < sizeof(query)) {
        httpd_req_get_url_query_str(req, query, sizeof(query));
        
        int index = -1;
        char index_str[8] = {0};
        
        if (httpd_query_key_value(query, "index", index_str, sizeof(index_str)) == ESP_OK) {
            index = atoi(index_str);
            cfg->remove_network(index);
            cfg->save_config();
            
            ESP_LOGI(TAG, "Network removed at index %d", index);
        }
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    char *json_str = cJSON_Print(response);
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(response);
    return err;
}

WebServer::WebServer() : server(nullptr) {
    if (g_discovered_ips_mutex == nullptr) {
        g_discovered_ips_mutex = xSemaphoreCreateMutex();
    }
}

WebServer::~WebServer() {
    stop();
}

void WebServer::discovery_task_handler(void *pvParameter) {
    ESP_LOGI(TAG, "[Discovery Task] Starting printer discovery...");
    
    // Log memory before discovery
    uint32_t free_heap_start = esp_get_free_heap_size();
    uint32_t free_heap_internal_start = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t free_heap_spiram_start = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "[Discovery Start] Total: %lu, Internal: %lu, SPIRAM: %lu", 
             free_heap_start, free_heap_internal_start, free_heap_spiram_start);
    
    g_discovery_in_progress = true;
    g_discovery_progress = 0;
    
    // Clear discovered IPs with mutex protection
    if (g_discovered_ips_mutex && xSemaphoreTake(g_discovered_ips_mutex, portMAX_DELAY)) {
        g_discovered_ips.clear();
        xSemaphoreGive(g_discovered_ips_mutex);
    }
    
    // Create lambda to update progress
    auto progress_callback = [](int current, int total) {
        g_discovery_progress = (total > 0) ? (current * 100) / total : 0;
        ESP_LOGD("WebServer", "[Discovery Progress] %d%%", g_discovery_progress);
    };
    
    // Set callback for real-time printer updates
    PrinterDiscovery::set_printer_found_callback([](const std::string& ip) {
        if (g_discovered_ips_mutex && xSemaphoreTake(g_discovered_ips_mutex, pdMS_TO_TICKS(100))) {
            if (std::find(g_discovered_ips.begin(), g_discovered_ips.end(), ip) == g_discovered_ips.end()) {
                g_discovered_ips.push_back(ip);
                ESP_LOGI(TAG, "[Real-time] Added printer: %s (total: %d)", ip.c_str(), g_discovered_ips.size());
            }
            xSemaphoreGive(g_discovered_ips_mutex);
        }
    });
    
    PrinterDiscovery discovery;
    // 60 second timeout for full subnet scans (254 IPs / 4 batch = 64 batches × 1s = 64s max)
    std::vector<PrinterDiscovery::PrinterInfo> results = discovery.discover(60000, progress_callback);
    
    // Store discovered printers with mutex protection
    if (g_discovered_ips_mutex && xSemaphoreTake(g_discovered_ips_mutex, portMAX_DELAY)) {
        for (const auto &printer : results) {
            // Check if not already added (avoid duplicates)
            if (std::find(g_discovered_ips.begin(), g_discovered_ips.end(), printer.ip_address) == g_discovered_ips.end()) {
                g_discovered_ips.push_back(printer.ip_address);
                ESP_LOGI(TAG, "[Discovery Result] Found printer: %s (%s)", printer.ip_address.c_str(), printer.hostname.c_str());
            }
        }
        xSemaphoreGive(g_discovered_ips_mutex);
    }
    
    g_discovery_progress = 100;
    g_discovery_in_progress = false;
    
    ESP_LOGI(TAG, "[Discovery Task] Discovery complete. Found %d printers", (int)results.size());
    
    // Log memory after discovery for debugging
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t free_heap_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    uint32_t free_heap_spiram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "[Discovery Cleanup] Total heap: %lu bytes, Internal: %lu bytes, SPIRAM: %lu bytes", 
             free_heap, free_heap_internal, free_heap_spiram);
    
    g_discovery_task = nullptr;
    vTaskDelete(NULL);
}

esp_err_t WebServer::start() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.max_uri_handlers = 22;  // Increased to accommodate all handlers (14 original + 3 network + 2 discovery + 2 more + 1 test)
    config.max_resp_headers = 16;  // Increase response header limit
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }
    
    // Register handlers
    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
    };
    httpd_register_uri_handler(server, &root);
    
    httpd_uri_t config_get = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = handle_api_config_get,
    };
    httpd_register_uri_handler(server, &config_get);
    
    httpd_uri_t config_post = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = handle_api_config_post,
    };
    httpd_register_uri_handler(server, &config_post);
    
    httpd_uri_t weather_get = {
        .uri = "/api/weather",
        .method = HTTP_GET,
        .handler = handle_api_weather_get,
    };
    httpd_register_uri_handler(server, &weather_get);
    
    httpd_uri_t weather_post = {
        .uri = "/api/weather",
        .method = HTTP_POST,
        .handler = handle_api_weather_post,
    };
    httpd_register_uri_handler(server, &weather_post);
    
    httpd_uri_t locations_get = {
        .uri = "/api/locations",
        .method = HTTP_GET,
        .handler = handle_api_locations_get,
    };
    httpd_register_uri_handler(server, &locations_get);
    
    httpd_uri_t locations_post = {
        .uri = "/api/locations",
        .method = HTTP_POST,
        .handler = handle_api_locations_post,
    };
    httpd_register_uri_handler(server, &locations_post);
    
    httpd_uri_t locations_delete = {
        .uri = "/api/locations",
        .method = HTTP_DELETE,
        .handler = handle_api_locations_delete,
    };
    httpd_register_uri_handler(server, &locations_delete);
    
    httpd_uri_t printers_get = {
        .uri = "/api/printers",
        .method = HTTP_GET,
        .handler = handle_api_printers_get,
    };
    httpd_register_uri_handler(server, &printers_get);
    
    httpd_uri_t printers_post = {
        .uri = "/api/printers",
        .method = HTTP_POST,
        .handler = handle_api_printers_post,
    };
    httpd_register_uri_handler(server, &printers_post);
    
    httpd_uri_t printers_delete = {
        .uri = "/api/printers",
        .method = HTTP_DELETE,
        .handler = handle_api_printers_delete,
    };
    httpd_register_uri_handler(server, &printers_delete);
    
    httpd_uri_t printers_discover = {
        .uri = "/api/printers/discover",
        .method = HTTP_GET,
        .handler = handle_api_printers_discover,
    };
    httpd_register_uri_handler(server, &printers_discover);
    
    httpd_uri_t printers_discover_post = {
        .uri = "/api/printers/discover",
        .method = HTTP_POST,
        .handler = handle_api_printers_discover,
    };
    httpd_register_uri_handler(server, &printers_discover_post);
    
    httpd_uri_t printers_discover_status = {
        .uri = "/api/printers/discover/status",
        .method = HTTP_GET,
        .handler = handle_api_printers_discover_status,
    };
    httpd_register_uri_handler(server, &printers_discover_status);
    
    httpd_uri_t printer_info = {
        .uri = "/api/printer/info",
        .method = HTTP_GET,
        .handler = handle_api_printer_info,
    };
    httpd_register_uri_handler(server, &printer_info);
    
    httpd_uri_t printer_query = {
        .uri = "/api/printer/query",
        .method = HTTP_GET,
        .handler = handle_api_printer_query,
    };
    httpd_register_uri_handler(server, &printer_query);
    
    httpd_uri_t test_connection = {
        .uri = "/api/test/connection",
        .method = HTTP_GET,
        .handler = handle_api_test_connection,
    };
    httpd_register_uri_handler(server, &test_connection);
    
    httpd_uri_t device_info = {
        .uri = "/api/device-info",
        .method = HTTP_GET,
        .handler = handle_api_device_info,
    };
    httpd_register_uri_handler(server, &device_info);
    
    httpd_uri_t networks_get = {
        .uri = "/api/networks",
        .method = HTTP_GET,
        .handler = handle_api_networks_get,
    };
    httpd_register_uri_handler(server, &networks_get);
    
    httpd_uri_t networks_post = {
        .uri = "/api/networks",
        .method = HTTP_POST,
        .handler = handle_api_networks_post,
    };
    httpd_register_uri_handler(server, &networks_post);
    
    httpd_uri_t networks_delete = {
        .uri = "/api/networks",
        .method = HTTP_DELETE,
        .handler = handle_api_networks_delete,
    };
    httpd_register_uri_handler(server, &networks_delete);
    
    ESP_LOGI(TAG, "HTTP server started on http://esp32-tux.local");
    return ESP_OK;
}

esp_err_t WebServer::stop() {
    if (server) {
        httpd_stop(server);
        server = nullptr;
    }
    return ESP_OK;
}

bool WebServer::is_running() {
    return server != nullptr;
}
