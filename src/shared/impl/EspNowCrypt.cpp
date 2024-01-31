#include <EspNowCrypt.h>
#include <cstring>
#include <esp_now.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>

#define KEY_SIZE_IN_BITS (16 * 8) // Assuming 16 bytes key size.
#define SECRET_LENGTH 8           // Hardcoded to 8 bytes.

#pragma pack(1)

/**
 * This is the most outer message, encaplsulating all other messages that are sent.
 * Data after this message contains the encryped data.
 * Size of this message and the following message cannot be larger than 250 bytes, which is the maximum for ESP NOW.
 */
struct EspNowEncryptionHeader {
  uint8_t iv[8];  // Random initialization vector.
  uint8_t tag[8]; // GCM authentication tag.
  uint8_t length; // length of the following payload buffer.
  // The encrypted payload is appended after this message, and is of length specified above.
};

#pragma pack(0)

EspNowCrypt::EspNowCrypt(const char *key, const char *secret) : _key(key), _secret(secret) {}

esp_err_t EspNowCrypt::sendMessage(const uint8_t *mac_addr, const void *input_message, const size_t input_length) {
  if (input_message == nullptr || input_length == 0) {
    // We need at least one byte to send.
    return ESP_ERR_INVALID_ARG;
  }

  // Generate a random 8 bytes IV.
  EspNowEncryptionHeader header;
  esp_fill_random(header.iv, sizeof(header.iv));

  // Total buffer to encrypt needs to be at least 16 bytes.
  // At the start of the buffer, we will include the _secret so we can
  // verify the integrity of the message.
  // So the size of the buffer to encrypt will be the input length + size of _secret
  // which we force to be SECRET_LENGTH.
  size_t total_length = SECRET_LENGTH + input_length;
  // We need at least 16 bytes, but we don't need a multiple of 16 bytes.
  // If smaller than 16, round up.
  header.length = std::max((size_t)16, total_length);
  std::unique_ptr<uint8_t[]> encrypted(new (std::nothrow) uint8_t[header.length]);
  if (encrypted == nullptr) {
    // Failed to allocate memory.
    return ESP_ERR_NO_MEM;
  }

  // Build the payload we want to encrypt by adding the secret first.
  // Then add the actual message.
  std::unique_ptr<uint8_t[]> input(new (std::nothrow) uint8_t[total_length]);
  if (input == nullptr) {
    // Failed to allocate memory.
    return ESP_ERR_NO_MEM;
  }
  std::memcpy(input.get(), _secret, SECRET_LENGTH);
  std::memcpy(input.get() + SECRET_LENGTH, input_message, input_length);

  mbedtls_gcm_context aes;
  mbedtls_gcm_init(&aes);
  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char *)_key, KEY_SIZE_IN_BITS);
  mbedtls_gcm_crypt_and_tag(&aes, MBEDTLS_GCM_ENCRYPT, header.length, header.iv, sizeof(header.iv), nullptr, 0,
                            input.get(), encrypted.get(), sizeof(header.tag), header.tag);
  mbedtls_gcm_free(&aes);

  // We now have or encrypted payload.
  // We want to send the outer EspNowEncryptionHeader followed by encrypted payload.
  size_t wire_length = sizeof(EspNowEncryptionHeader) + header.length;
  std::unique_ptr<uint8_t[]> wire_buffer(new (std::nothrow) uint8_t[wire_length]);
  if (wire_buffer == nullptr) {
    // Failed to allocate memory.
    return ESP_ERR_NO_MEM;
  }
  std::memcpy(wire_buffer.get(), &header, sizeof(EspNowEncryptionHeader));
  std::memcpy(wire_buffer.get() + sizeof(EspNowEncryptionHeader), encrypted.get(), header.length);

  esp_err_t r = esp_now_send(mac_addr, wire_buffer.get(), wire_length);

  return r;
}

std::unique_ptr<uint8_t[]> EspNowCrypt::decryptMessage(const void *input_message) {
  EspNowEncryptionHeader *header = (EspNowEncryptionHeader *)input_message;

  // If we have no secret, something is off. No point in continue at all in that case.
  // Or if we ONLY have a secret. Its technically valid, but also off.
  if (header->length <= SECRET_LENGTH) {
    return nullptr;
  }

  std::unique_ptr<uint8_t[]> decrypted(new (std::nothrow) uint8_t[header->length]);
  if (decrypted == nullptr) {
    // Failed to allocate memory.
    return nullptr;
  }
  uint8_t *encrypted = (uint8_t *)input_message + sizeof(EspNowEncryptionHeader);

  mbedtls_gcm_context aes;
  mbedtls_gcm_init(&aes);
  mbedtls_gcm_setkey(&aes, MBEDTLS_CIPHER_ID_AES, (const unsigned char *)_key, KEY_SIZE_IN_BITS);
  int r = mbedtls_gcm_crypt_and_tag(&aes, MBEDTLS_GCM_DECRYPT, header->length, header->iv, sizeof(header->iv), nullptr,
                                    0, encrypted, decrypted.get(), sizeof(header->tag), header->tag);
  mbedtls_gcm_free(&aes);

  if (r != 0) {
    // Decryption failed.
    return nullptr;
  }

  // Verify secret. Its always first.
  if (memcmp(decrypted.get(), _secret, SECRET_LENGTH) != 0) {
    // Secret not valid.
    return nullptr;
  }

  // Copy message without secret so we can return a pointer to something that can be free:d.
  size_t output_message_length = header->length - SECRET_LENGTH;
  std::unique_ptr<uint8_t[]> output_message(new (std::nothrow) uint8_t[output_message_length]);
  if (output_message == nullptr) {
    // Failed to allocate memory.
    return nullptr;
  }
  std::memcpy(output_message.get(), decrypted.get() + SECRET_LENGTH, output_message_length);

  return output_message;
}
