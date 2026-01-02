/**
 * @file crypto.h
 * @brief Simple encryption for credential storage
 * 
 * Uses XOR encryption with a device-unique key derived from 
 * the ESP32's MAC address. This provides protection against
 * casual reading of the flash memory.
 */

#ifndef CRYPTO_H
#define CRYPTO_H

#include <Arduino.h>
#include <WiFi.h>

// ============================================================
// Encryption Key Generation
// ============================================================

/**
 * Generate a device-unique encryption key from MAC address
 * The key is 16 bytes, derived from the 6-byte MAC + padding
 */
static void getEncryptionKey(uint8_t* key) {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  
  // Create 16-byte key from MAC address with mixing
  for (int i = 0; i < 16; i++) {
    key[i] = mac[i % 6] ^ (0xA5 + i * 0x17) ^ ((i * 31) & 0xFF);
  }
}

// ============================================================
// XOR Encryption/Decryption
// ============================================================

/**
 * Encrypt a string using XOR with device-unique key
 * Returns Base64-encoded encrypted string
 */
static String encryptString(const String& plaintext) {
  if (plaintext.length() == 0) return "";
  
  uint8_t key[16];
  getEncryptionKey(key);
  
  // XOR encrypt
  size_t len = plaintext.length();
  uint8_t* encrypted = new uint8_t[len];
  
  for (size_t i = 0; i < len; i++) {
    encrypted[i] = plaintext[i] ^ key[i % 16];
  }
  
  // Convert to hex string (simple and safe)
  String result = "";
  for (size_t i = 0; i < len; i++) {
    char hex[3];
    sprintf(hex, "%02X", encrypted[i]);
    result += hex;
  }
  
  delete[] encrypted;
  return result;
}

/**
 * Decrypt a hex-encoded encrypted string
 */
static String decryptString(const String& ciphertext) {
  if (ciphertext.length() == 0) return "";
  
  // Check if it looks like encrypted data (hex string)
  // If not, assume it's old unencrypted data and return as-is
  bool isHex = true;
  for (size_t i = 0; i < ciphertext.length(); i++) {
    char c = ciphertext[i];
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
      isHex = false;
      break;
    }
  }
  
  // If not hex or odd length, return as-is (backward compatibility)
  if (!isHex || ciphertext.length() % 2 != 0) {
    return ciphertext;
  }
  
  uint8_t key[16];
  getEncryptionKey(key);
  
  // Convert from hex string
  size_t len = ciphertext.length() / 2;
  uint8_t* encrypted = new uint8_t[len];
  
  for (size_t i = 0; i < len; i++) {
    String byteStr = ciphertext.substring(i * 2, i * 2 + 2);
    encrypted[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
  }
  
  // XOR decrypt
  String result = "";
  for (size_t i = 0; i < len; i++) {
    char decrypted = encrypted[i] ^ key[i % 16];
    // Sanity check - if result doesn't look like text, return original
    if (decrypted < 32 || decrypted > 126) {
      delete[] encrypted;
      return ciphertext;  // Return as-is (probably old unencrypted data)
    }
    result += decrypted;
  }
  
  delete[] encrypted;
  return result;
}

#endif // CRYPTO_H
