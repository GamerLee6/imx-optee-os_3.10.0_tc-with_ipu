#ifndef CRYPTSHA_H
#define CRYPTSHA_H

#include <utee_defines.h>
#include <tee_api_types.h>

#define TEE_KEY_SIZE           	16
#define TEE_SHA256_HASH_SIZE    32
#define TEE_PLAIN_SIZE          64
#define TEE_BLOB_SIZE           TEE_PLAIN_SIZE + 32 + 16

void back_blob(paddr_t);
TEE_Result get_aes_key(paddr_t);
TEE_Result tee_aes_cbc_crypt(uint8_t *, uint8_t *, size_t, uint8_t *, TEE_OperationMode);


#endif /* CRYPTSHA_H */