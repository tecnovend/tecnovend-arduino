#include "json_utils.h"

String urlEncode(const String& value) {
  String encoded;
  const char* hex = "0123456789ABCDEF";

  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    bool safe = (c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') ||
                c == '-' || c == '_' || c == '.' || c == '~';

    if (safe) {
      encoded += c;
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }

  return encoded;
}

String jsonEscape(const String& value) {
  String escaped;

  for (size_t i = 0; i < value.length(); i++) {
    char c = value[i];
    if (c == '"' || c == '\\') {
      escaped += '\\';
      escaped += c;
    } else if (c == '\n') {
      escaped += "\\n";
    } else if (c == '\r') {
      escaped += "\\r";
    } else if (c == '\t') {
      escaped += "\\t";
    } else {
      escaped += c;
    }
  }

  return escaped;
}

String extractStringField(const String& object, const char* fieldName) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = object.indexOf(key);
  if (keyPos < 0) return "";

  int colon = object.indexOf(':', keyPos + key.length());
  int firstQuote = object.indexOf('"', colon + 1);
  int secondQuote = object.indexOf('"', firstQuote + 1);
  if (colon < 0 || firstQuote < 0 || secondQuote < 0) return "";

  return object.substring(firstQuote + 1, secondQuote);
}

int extractIntField(const String& object, const char* fieldName, int fallback) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = object.indexOf(key);
  if (keyPos < 0) return fallback;

  int colon = object.indexOf(':', keyPos + key.length());
  if (colon < 0) return fallback;

  int start = colon + 1;
  while (start < object.length() && isspace(object[start])) start++;

  int end = start;
  while (end < object.length() && isdigit(object[end])) end++;

  if (end == start) return fallback;
  return object.substring(start, end).toInt();
}

String extractObjectField(const String& json, const char* fieldName) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = json.indexOf(key);
  if (keyPos < 0) return "";

  int colon = json.indexOf(':', keyPos + key.length());
  int objStart = json.indexOf('{', colon + 1);
  if (colon < 0 || objStart < 0) return "";

  int depth = 0;
  for (int i = objStart; i < json.length(); i++) {
    if (json[i] == '{') depth++;
    if (json[i] == '}') depth--;
    if (depth == 0) {
      return json.substring(objStart, i + 1);
    }
  }

  return "";
}

bool parseBoolField(const String& object, const char* fieldName, bool fallback) {
  String key = String("\"") + fieldName + "\"";
  int keyPos = object.indexOf(key);
  if (keyPos < 0) return fallback;

  int colon = object.indexOf(':', keyPos + key.length());
  if (colon < 0) return fallback;

  int start = colon + 1;
  while (start < object.length() && isspace(object[start])) start++;

  if (object.substring(start, start + 4) == "true") return true;
  if (object.substring(start, start + 5) == "false") return false;
  return fallback;
}

int parsePulses(const String& json, Pulse* pulses, int maxPulses) {
  int arrayKey = json.indexOf("\"pending_pulses\"");
  if (arrayKey < 0) return 0;

  int arrayStart = json.indexOf('[', arrayKey);
  int arrayEnd = json.indexOf(']', arrayStart);
  if (arrayStart < 0 || arrayEnd < 0 || arrayEnd <= arrayStart) return 0;

  int count = 0;
  int pos = arrayStart;

  while (count < maxPulses) {
    int objStart = json.indexOf('{', pos);
    if (objStart < 0 || objStart > arrayEnd) break;

    int objEnd = json.indexOf('}', objStart);
    if (objEnd < 0 || objEnd > arrayEnd) break;

    String object = json.substring(objStart, objEnd + 1);

    Pulse pulse;
    pulse.id = extractStringField(object, "pulse_id");
    pulse.channel = extractIntField(object, "channel", 1);
    pulse.count = extractIntField(object, "count", 0);

    if (pulse.id.length() > 0 && pulse.channel > 0 && pulse.count > 0) {
      pulses[count++] = pulse;
    }

    pos = objEnd + 1;
  }

  return count;
}
