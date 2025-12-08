/**
 * @file lang.hpp
 * @brief Internationalization (i18n) support for ESP32-TUX
 * 
 * Supported languages: EN, DE, NL, PL, RU
 * 
 * Usage:
 *   #include "i18n/lang.hpp"
 *   const char* text = TR(STR_IDLE);  // Returns translated string
 *   set_language(LANG_DE);            // Switch to German
 */

#pragma once

#include <cstdint>

// Language codes
typedef enum {
    LANG_EN = 0,  // English (default)
    LANG_DE,      // German (Deutsch)
    LANG_NL,      // Dutch (Nederlands)
    LANG_PL,      // Polish (Polski)
    LANG_RU,      // Russian (Русский)
    LANG_COUNT
} language_t;

// String IDs
typedef enum {
    // Printer states
    STR_IDLE = 0,
    STR_RUNNING,
    STR_PRINTING,
    STR_PAUSED,
    STR_FINISHED,
    STR_ERROR,
    STR_FAILED,
    STR_OFFLINE,
    STR_ONLINE,
    
    // Printer labels
    STR_BED,
    STR_LAYER,
    STR_NOZZLE,
    STR_FILE,
    STR_PROGRESS,
    
    // Time formatting
    STR_HOURS_SHORT,     // "h"
    STR_MINUTES_SHORT,   // "m"
    STR_TIME_LEFT,       // "left" or "remaining"
    
    // Welcome/Placeholder
    STR_WELCOME_TITLE,
    STR_WELCOME_SUBTITLE,
    STR_WELCOME_MSG,
    
    // Settings page
    STR_SETTINGS,
    STR_WIFI,
    STR_WIFI_CONNECT,
    STR_WIFI_DISCONNECT,
    STR_WIFI_CONNECTED,
    STR_WIFI_DISCONNECTED,
    STR_WIFI_CONNECTING,
    
    // Updates page
    STR_UPDATES,
    STR_CHECK_UPDATES,
    STR_UPDATE_AVAILABLE,
    STR_UP_TO_DATE,
    STR_UPDATING,
    
    // Device info
    STR_DEVICE_INFO,
    STR_VERSION,
    STR_FREE_HEAP,
    STR_UPTIME,
    
    // Weather
    STR_WEATHER,
    STR_TEMPERATURE,
    STR_HUMIDITY,
    STR_WIND,
    STR_PRESSURE,
    STR_FEELS_LIKE,
    STR_HIGH,              // "H" or "Max"
    STR_LOW,               // "L" or "Min"
    STR_LOADING_WEATHER,   // "Loading weather..."
    STR_API_KEY_MISSING,   // "Weather API key not configured"
    STR_GO_TO_SETTINGS,    // "Go to http://esp32-tux.local"
    STR_ADD_API_KEY,       // "Settings > Weather to add your API key"
    
    // Common
    STR_YES,
    STR_NO,
    STR_OK,
    STR_CANCEL,
    STR_SAVE,
    STR_BACK,
    STR_NEXT,
    STR_LOADING,
    
    STR_COUNT  // Must be last
} string_id_t;

// Current language (default English)
static language_t current_language = LANG_EN;

// Translation tables
static const char* const translations[LANG_COUNT][STR_COUNT] = {
    // LANG_EN - English
    {
        "IDLE",           // STR_IDLE
        "RUNNING",        // STR_RUNNING
        "PRINTING",       // STR_PRINTING
        "PAUSED",         // STR_PAUSED
        "FINISHED",       // STR_FINISHED
        "ERROR",          // STR_ERROR
        "FAILED",         // STR_FAILED
        "Offline",        // STR_OFFLINE
        "Online",         // STR_ONLINE
        "Bed",            // STR_BED
        "Layer",          // STR_LAYER
        "Nozzle",         // STR_NOZZLE
        "File",           // STR_FILE
        "Progress",       // STR_PROGRESS
        "h",              // STR_HOURS_SHORT
        "m",              // STR_MINUTES_SHORT
        "left",           // STR_TIME_LEFT
        "Welcome to TUX", // STR_WELCOME_TITLE
        "Add locations & printers", // STR_WELCOME_SUBTITLE
        "to see more info", // STR_WELCOME_MSG
        "Settings",       // STR_SETTINGS
        "WiFi",           // STR_WIFI
        "Connect",        // STR_WIFI_CONNECT
        "Disconnect",     // STR_WIFI_DISCONNECT
        "Connected",      // STR_WIFI_CONNECTED
        "Disconnected",   // STR_WIFI_DISCONNECTED
        "Connecting...",  // STR_WIFI_CONNECTING
        "Updates",        // STR_UPDATES
        "Check for Updates", // STR_CHECK_UPDATES
        "Update Available", // STR_UPDATE_AVAILABLE
        "Up to Date",     // STR_UP_TO_DATE
        "Updating...",    // STR_UPDATING
        "Device Info",    // STR_DEVICE_INFO
        "Version",        // STR_VERSION
        "Free Heap",      // STR_FREE_HEAP
        "Uptime",         // STR_UPTIME
        "Weather",        // STR_WEATHER
        "Temperature",    // STR_TEMPERATURE
        "Humidity",       // STR_HUMIDITY
        "Wind",           // STR_WIND
        "Pressure",       // STR_PRESSURE
        "Feels like",     // STR_FEELS_LIKE
        "H",              // STR_HIGH
        "L",              // STR_LOW
        "Loading weather...", // STR_LOADING_WEATHER
        "Weather API key not configured", // STR_API_KEY_MISSING
        "Go to http://esp32-tux.local", // STR_GO_TO_SETTINGS
        "Settings > Weather to add your API key", // STR_ADD_API_KEY
        "Yes",            // STR_YES
        "No",             // STR_NO
        "OK",             // STR_OK
        "Cancel",         // STR_CANCEL
        "Save",           // STR_SAVE
        "Back",           // STR_BACK
        "Next",           // STR_NEXT
        "Loading...",     // STR_LOADING
    },
    
    // LANG_DE - German (Deutsch)
    {
        "BEREIT",         // STR_IDLE
        "LÄUFT",          // STR_RUNNING
        "DRUCKT",         // STR_PRINTING
        "PAUSIERT",       // STR_PAUSED
        "FERTIG",         // STR_FINISHED
        "FEHLER",         // STR_ERROR
        "FEHLGESCHLAGEN", // STR_FAILED
        "Offline",        // STR_OFFLINE
        "Online",         // STR_ONLINE
        "Bett",           // STR_BED
        "Schicht",        // STR_LAYER
        "Düse",           // STR_NOZZLE
        "Datei",          // STR_FILE
        "Fortschritt",    // STR_PROGRESS
        "Std",            // STR_HOURS_SHORT
        "Min",            // STR_MINUTES_SHORT
        "verbleibend",    // STR_TIME_LEFT
        "Willkommen bei TUX", // STR_WELCOME_TITLE
        "Standorte & Drucker hinzufügen", // STR_WELCOME_SUBTITLE
        "für mehr Infos", // STR_WELCOME_MSG
        "Einstellungen",  // STR_SETTINGS
        "WLAN",           // STR_WIFI
        "Verbinden",      // STR_WIFI_CONNECT
        "Trennen",        // STR_WIFI_DISCONNECT
        "Verbunden",      // STR_WIFI_CONNECTED
        "Getrennt",       // STR_WIFI_DISCONNECTED
        "Verbinde...",    // STR_WIFI_CONNECTING
        "Updates",        // STR_UPDATES
        "Nach Updates suchen", // STR_CHECK_UPDATES
        "Update verfügbar", // STR_UPDATE_AVAILABLE
        "Aktuell",        // STR_UP_TO_DATE
        "Aktualisiere...", // STR_UPDATING
        "Geräte-Info",    // STR_DEVICE_INFO
        "Version",        // STR_VERSION
        "Freier Speicher", // STR_FREE_HEAP
        "Laufzeit",       // STR_UPTIME
        "Wetter",         // STR_WEATHER
        "Temperatur",     // STR_TEMPERATURE
        "Luftfeuchtigkeit", // STR_HUMIDITY
        "Wind",           // STR_WIND
        "Luftdruck",      // STR_PRESSURE
        "Gefühlt",        // STR_FEELS_LIKE
        "H",              // STR_HIGH
        "T",              // STR_LOW (Tief)
        "Wetter wird geladen...", // STR_LOADING_WEATHER
        "Wetter-API-Schlüssel fehlt", // STR_API_KEY_MISSING
        "Gehe zu http://esp32-tux.local", // STR_GO_TO_SETTINGS
        "Einstellungen > Wetter", // STR_ADD_API_KEY
        "Ja",             // STR_YES
        "Nein",           // STR_NO
        "OK",             // STR_OK
        "Abbrechen",      // STR_CANCEL
        "Speichern",      // STR_SAVE
        "Zurück",         // STR_BACK
        "Weiter",         // STR_NEXT
        "Lädt...",        // STR_LOADING
    },
    
    // LANG_NL - Dutch (Nederlands)
    {
        "INACTIEF",       // STR_IDLE
        "ACTIEF",         // STR_RUNNING
        "PRINTEN",        // STR_PRINTING
        "GEPAUZEERD",     // STR_PAUSED
        "VOLTOOID",       // STR_FINISHED
        "FOUT",           // STR_ERROR
        "MISLUKT",        // STR_FAILED
        "Offline",        // STR_OFFLINE
        "Online",         // STR_ONLINE
        "Bed",            // STR_BED
        "Laag",           // STR_LAYER
        "Nozzle",         // STR_NOZZLE
        "Bestand",        // STR_FILE
        "Voortgang",      // STR_PROGRESS
        "u",              // STR_HOURS_SHORT
        "m",              // STR_MINUTES_SHORT
        "resterend",      // STR_TIME_LEFT
        "Welkom bij TUX", // STR_WELCOME_TITLE
        "Voeg locaties & printers toe", // STR_WELCOME_SUBTITLE
        "voor meer info", // STR_WELCOME_MSG
        "Instellingen",   // STR_SETTINGS
        "WiFi",           // STR_WIFI
        "Verbinden",      // STR_WIFI_CONNECT
        "Verbreken",      // STR_WIFI_DISCONNECT
        "Verbonden",      // STR_WIFI_CONNECTED
        "Verbroken",      // STR_WIFI_DISCONNECTED
        "Verbinden...",   // STR_WIFI_CONNECTING
        "Updates",        // STR_UPDATES
        "Controleer op updates", // STR_CHECK_UPDATES
        "Update beschikbaar", // STR_UPDATE_AVAILABLE
        "Actueel",        // STR_UP_TO_DATE
        "Bijwerken...",   // STR_UPDATING
        "Apparaat Info",  // STR_DEVICE_INFO
        "Versie",         // STR_VERSION
        "Vrij Geheugen",  // STR_FREE_HEAP
        "Uptime",         // STR_UPTIME
        "Weer",           // STR_WEATHER
        "Temperatuur",    // STR_TEMPERATURE
        "Vochtigheid",    // STR_HUMIDITY
        "Wind",           // STR_WIND
        "Luchtdruk",      // STR_PRESSURE
        "Voelt als",      // STR_FEELS_LIKE
        "H",              // STR_HIGH
        "L",              // STR_LOW
        "Weer wordt geladen...", // STR_LOADING_WEATHER
        "Weer API-sleutel niet ingesteld", // STR_API_KEY_MISSING
        "Ga naar http://esp32-tux.local", // STR_GO_TO_SETTINGS
        "Instellingen > Weer", // STR_ADD_API_KEY
        "Ja",             // STR_YES
        "Nee",            // STR_NO
        "OK",             // STR_OK
        "Annuleren",      // STR_CANCEL
        "Opslaan",        // STR_SAVE
        "Terug",          // STR_BACK
        "Volgende",       // STR_NEXT
        "Laden...",       // STR_LOADING
    },
    
    // LANG_PL - Polish (Polski) - with proper diacritics
    {
        "BEZCZYNNY",      // STR_IDLE
        "DZIAŁA",         // STR_RUNNING
        "DRUKUJE",        // STR_PRINTING
        "WSTRZYMANO",     // STR_PAUSED
        "ZAKOŃCZONE",     // STR_FINISHED
        "BŁĄD",           // STR_ERROR
        "NIEPOWODZENIE",  // STR_FAILED
        "Offline",        // STR_OFFLINE
        "Online",         // STR_ONLINE
        "Stół",           // STR_BED
        "Warstwa",        // STR_LAYER
        "Dysza",          // STR_NOZZLE
        "Plik",           // STR_FILE
        "Postęp",         // STR_PROGRESS
        "godz",           // STR_HOURS_SHORT
        "min",            // STR_MINUTES_SHORT
        "pozostało",      // STR_TIME_LEFT
        "Witamy w TUX",   // STR_WELCOME_TITLE
        "Dodaj lokalizacje i drukarki", // STR_WELCOME_SUBTITLE
        "aby zobaczyć więcej", // STR_WELCOME_MSG
        "Ustawienia",     // STR_SETTINGS
        "WiFi",           // STR_WIFI
        "Połącz",         // STR_WIFI_CONNECT
        "Rozłącz",        // STR_WIFI_DISCONNECT
        "Połączono",      // STR_WIFI_CONNECTED
        "Rozłączono",     // STR_WIFI_DISCONNECTED
        "Łączenie...",    // STR_WIFI_CONNECTING
        "Aktualizacje",   // STR_UPDATES
        "Sprawdź aktualizacje", // STR_CHECK_UPDATES
        "Dostępna aktualizacja", // STR_UPDATE_AVAILABLE
        "Aktualny",       // STR_UP_TO_DATE
        "Aktualizowanie...", // STR_UPDATING
        "Info o urządzeniu", // STR_DEVICE_INFO
        "Wersja",         // STR_VERSION
        "Wolna pamięć",   // STR_FREE_HEAP
        "Czas pracy",     // STR_UPTIME
        "Pogoda",         // STR_WEATHER
        "Temperatura",    // STR_TEMPERATURE
        "Wilgotność",     // STR_HUMIDITY
        "Wiatr",          // STR_WIND
        "Ciśnienie",      // STR_PRESSURE
        "Odczuwalna",     // STR_FEELS_LIKE
        "Maks",           // STR_HIGH
        "Min",            // STR_LOW
        "Ładowanie pogody...", // STR_LOADING_WEATHER
        "Brak klucza API pogody", // STR_API_KEY_MISSING
        "Przejdź do http://esp32-tux.local", // STR_GO_TO_SETTINGS
        "Ustawienia > Pogoda", // STR_ADD_API_KEY
        "Tak",            // STR_YES
        "Nie",            // STR_NO
        "OK",             // STR_OK
        "Anuluj",         // STR_CANCEL
        "Zapisz",         // STR_SAVE
        "Wstecz",         // STR_BACK
        "Dalej",          // STR_NEXT
        "Ładowanie...",   // STR_LOADING
    },
    
    // LANG_RU - Russian (Русский)
    {
        "ОЖИДАНИЕ",       // STR_IDLE
        "РАБОТАЕТ",       // STR_RUNNING
        "ПЕЧАТЬ",         // STR_PRINTING
        "ПАУЗА",          // STR_PAUSED
        "ГОТОВО",         // STR_FINISHED
        "ОШИБКА",         // STR_ERROR
        "СБОЙ",           // STR_FAILED
        "Не в сети",      // STR_OFFLINE
        "В сети",         // STR_ONLINE
        "Стол",           // STR_BED
        "Слой",           // STR_LAYER
        "Сопло",          // STR_NOZZLE
        "Файл",           // STR_FILE
        "Прогресс",       // STR_PROGRESS
        "ч",              // STR_HOURS_SHORT
        "м",              // STR_MINUTES_SHORT
        "осталось",       // STR_TIME_LEFT
        "Добро пожаловать", // STR_WELCOME_TITLE
        "Добавьте локации и принтеры", // STR_WELCOME_SUBTITLE
        "для информации", // STR_WELCOME_MSG
        "Настройки",      // STR_SETTINGS
        "WiFi",           // STR_WIFI
        "Подключить",     // STR_WIFI_CONNECT
        "Отключить",      // STR_WIFI_DISCONNECT
        "Подключено",     // STR_WIFI_CONNECTED
        "Отключено",      // STR_WIFI_DISCONNECTED
        "Подключение...", // STR_WIFI_CONNECTING
        "Обновления",     // STR_UPDATES
        "Проверить обновления", // STR_CHECK_UPDATES
        "Доступно обновление", // STR_UPDATE_AVAILABLE
        "Актуально",      // STR_UP_TO_DATE
        "Обновление...",  // STR_UPDATING
        "Информация",     // STR_DEVICE_INFO
        "Версия",         // STR_VERSION
        "Свободная память", // STR_FREE_HEAP
        "Время работы",   // STR_UPTIME
        "Погода",         // STR_WEATHER
        "Температура",    // STR_TEMPERATURE
        "Влажность",      // STR_HUMIDITY
        "Ветер",          // STR_WIND
        "Давление",       // STR_PRESSURE
        "Ощущается как",  // STR_FEELS_LIKE
        "Макс",           // STR_HIGH
        "Мин",            // STR_LOW
        "Загрузка погоды...", // STR_LOADING_WEATHER
        "API ключ погоды не настроен", // STR_API_KEY_MISSING
        "Перейдите на http://esp32-tux.local", // STR_GO_TO_SETTINGS
        "Настройки > Погода", // STR_ADD_API_KEY
        "Да",             // STR_YES
        "Нет",            // STR_NO
        "OK",             // STR_OK
        "Отмена",         // STR_CANCEL
        "Сохранить",      // STR_SAVE
        "Назад",          // STR_BACK
        "Далее",          // STR_NEXT
        "Загрузка...",    // STR_LOADING
    },
};

/**
 * @brief Get translated string for current language
 * @param id String ID
 * @return Translated string (never NULL)
 */
inline const char* TR(string_id_t id) {
    if (id >= STR_COUNT) return "";
    return translations[current_language][id];
}

/**
 * @brief Set current language
 * @param lang Language code
 */
inline void set_language(language_t lang) {
    if (lang < LANG_COUNT) {
        current_language = lang;
    }
}

/**
 * @brief Get current language
 * @return Current language code
 */
inline language_t get_language() {
    return current_language;
}

/**
 * @brief Get language name
 * @param lang Language code
 * @return Language name string
 */
inline const char* get_language_name(language_t lang) {
    static const char* names[] = {
        "English",
        "Deutsch",
        "Nederlands",
        "Polski",
        "Русский"
    };
    if (lang < LANG_COUNT) return names[lang];
    return "Unknown";
}

/**
 * @brief Get language code string (2-letter)
 * @param lang Language code
 * @return 2-letter language code
 */
inline const char* get_language_code(language_t lang) {
    static const char* codes[] = { "EN", "DE", "NL", "PL", "RU" };
    if (lang < LANG_COUNT) return codes[lang];
    return "??";
}

/**
 * @brief Set language from 2-letter code string
 * @param code 2-letter language code (case insensitive)
 * @return true if language was set, false if unknown code
 */
inline bool set_language_from_code(const char* code) {
    if (!code) return false;
    
    // Convert to uppercase for comparison
    char upper[3] = {0};
    upper[0] = (code[0] >= 'a' && code[0] <= 'z') ? code[0] - 32 : code[0];
    upper[1] = (code[1] >= 'a' && code[1] <= 'z') ? code[1] - 32 : code[1];
    
    if (strcmp(upper, "EN") == 0) { set_language(LANG_EN); return true; }
    if (strcmp(upper, "DE") == 0) { set_language(LANG_DE); return true; }
    if (strcmp(upper, "NL") == 0) { set_language(LANG_NL); return true; }
    if (strcmp(upper, "PL") == 0) { set_language(LANG_PL); return true; }
    if (strcmp(upper, "RU") == 0) { set_language(LANG_RU); return true; }
    
    return false;
}
