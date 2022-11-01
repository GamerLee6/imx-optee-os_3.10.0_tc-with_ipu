#include <assert.h>
#include <stdbool.h>
#include <trace.h>
#include <kernel/panic.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <tee/tee_cryp_utl.h>
#include <util.h>
#include <crypto/crypto.h>
#include <utee_defines.h>
#include <string.h>

/* Library NXP includes */

#include <tee/crypt_sha.h>

#include <tee_internal_api.h>

uint8_t key[TEE_KEY_SIZE] = {0x71, 0x01, 0x3c, 0x04, 0x27, 0x2a, 0xa4, 0x38, \
				0x2b, 0xe4, 0x32, 0x63, 0x8e, 0xc6, 0xc4, 0x8e};

// Use NXP data sealing lib to derive the symmetric encryption key from the CAAM blob
// TEE_Result get_aes_key(paddr_t blob_pa)
// {
//   uint8_t key_derivation[TEE_KEY_SIZE] = {
//     0x71, 0x01, 0x3c, 0x04, 0x27, 0x2a, 0xa4, 0x38,
// 		0x2b, 0xe4, 0x32, 0x63, 0x8e, 0xc6, 0xc4, 0x8e
//   };

//   uint8_t blob_buffer[64] = {
//     0xdd, 0x1d, 0x6a, 0xe4, 0xb1, 0xf8, 0xa4, 0x92, 
//     0xf2, 0xe0, 0xcd, 0x10, 0x6c, 0x5d, 0x93, 0xcd,
//     0xdc, 0xb0, 0x57, 0xea, 0x5e, 0xb5, 0x24, 0xed,  
//     0xc0, 0x26, 0x73, 0x10, 0x71, 0xc4, 0x1b, 0x3b,
//     0xf9, 0x1f, 0xdb, 0xe9, 0x8a, 0x7a, 0x16, 0xe1, 
//     0xe3, 0x47, 0xbb, 0x08, 0x19, 0x30, 0xaf, 0xa6,
//     0xb8, 0x1b, 0xb5, 0xe4, 0x46, 0xf4, 0xa8, 0x81, 
//     0xda, 0x35, 0x1f, 0x82, 0x32, 0x1c, 0xc6, 0x9f
//   };

//   struct nxpcrypt_buf de_payload;
//   de_payload.data = key;
//   de_payload.length = TEE_KEY_SIZE;

//   struct nxpcrypt_buf blob;
//   blob.data = blob_buffer;
//   blob.length = 64;

//   TEE_Result dec_res = blob_decapsulate(RED, key_derivation, &de_payload, &blob);
//   if(dec_res == TEE_SUCCESS)
//   {
//     DMSG("Remote Attestation Key Blob Decapsulation Succeeds.");
//   }

// 	return dec_res;
// }

// Use the symmetric encryption key derived from CAAM blob to encrypt the NW image measurement result
TEE_Result tee_aes_cbc_crypt(uint8_t *in, uint8_t *out, size_t size, uint8_t *iv_src, TEE_OperationMode mode){
	TEE_Result res;
	void *ctx;
	uint8_t iv[TEE_AES_BLOCK_SIZE] = {0};
	uint8_t digest[TEE_SHA256_HASH_SIZE];
	const uint32_t algo = TEE_ALG_AES_CBC_NOPAD;

	DMSG("aes-128-cbc %scrypt", (mode == TEE_MODE_ENCRYPT) ? "En" : "De");
	
	/* Compute initialization vector for this block */
	tee_hash_createdigest(TEE_ALG_SHA256, iv_src, strlen((char *)iv_src), digest, sizeof(digest));
	memmove(iv, digest, TEE_AES_BLOCK_SIZE);

	/* Run AES CBC */
	res = crypto_cipher_alloc_ctx(&ctx, algo);
	if (res != TEE_SUCCESS)
		return res;

	res = crypto_cipher_init(ctx, mode, key, TEE_KEY_SIZE, NULL,
				 0, iv, TEE_AES_BLOCK_SIZE);
	if (res != TEE_SUCCESS){
		DMSG("crypto_cipher_init wrong");
		goto exit;
	}
	res = crypto_cipher_update(ctx, mode, true, in, size, out);
	if (res != TEE_SUCCESS){
		DMSG("crypto_cipher_update wrong");
		goto exit;
	}
	crypto_cipher_final(ctx);

exit:
	crypto_cipher_free_ctx(ctx);
	return res;
}
