
#ifndef GET_TASK_INFO_H
#define GET_TASK_INFO_H

#include <tee_api_types.h>
#include <tee_api_defines.h>
#include "shaDM.h"

#define FILE_NUM 32 // need modifying
#define MAX_PAGE_COUNT 198

#define PGDIR_SHIFT 21
#define PAGE_SHIFT 12
#define PAGE_MASK (~((1 << PAGE_SHIFT) - 1))
#define PTRS_PER_PTE 512
#define PTRS_PER_PGD 2048

#define pgd_index(addr) ((addr) >> PGDIR_SHIFT);
#define pte_index(addr) (((addr) >> PAGE_SHIFT) & (PTRS_PER_PTE - 1))

enum ta_types {
	GET_TRUSTED_BOOT_RESULT = 0,
	PRINT_NSEC_TASK_FILE_INFOS,
	PRINT_NSEC_TASK_MM_INFOS,
	MEM_UWORLD_HASH_REGION
};

struct task_struct_param {
	paddr_t init_task; 
	uint32_t va2pa_offset;
	uint32_t pid_offset;
	uint32_t comm_offset;
	uint32_t tasks_offset;
	uint32_t files_offset;
	uint32_t fdt_offset;		//in files_struct
	uint32_t fd_array_offset;	//in files_struct
	uint32_t max_fds_offset;	//in fdtable
	uint32_t fd_offset;		//in fdtable
	uint32_t f_path_offset;		//in file
	uint32_t f_mode_offset;		//in file
	uint32_t private_data_offset;	//in file
	uint32_t dentry_offset;		//in path
	uint32_t d_parent_offset;	//in dentry
	uint32_t d_iname_offset;	//in dentry
	uint32_t d_name_offset;		//in dentry
	uint32_t name_offset;		//in qstr
	uint32_t type_offset;		//in socket
	uint32_t sk_offset;		//in socket
	uint32_t sk_common_offset;	//in sock
	uint32_t skc_daddr_offset;	//in sock_common, remote IPv4 addr
	uint32_t skc_rcv_saddr_offset;	//in sock_common, local IPv4 addr
	uint32_t skc_v6_daddr_offset;	//in sock_common, remote IPv6 addr
	uint32_t skc_v6_rcv_saddr_offset;	//in sock_common, local IPv6 addr
	uint32_t skc_dport_offset;		//in sock_common, remote port
	uint32_t skc_num_offset;		//in sock_common, local port
	uint32_t mm_offset;
	uint32_t pgd_offset;	//in mm_struct
	uint32_t scode_offset;	//in mm_struct
	uint32_t ecode_offset;	//in mm_struct
};

void pathcat(char *dest, char *src);
void HexArrayToString(char *string, uint8_t *hexarray, int length);
void *read_sock(char *sockname, paddr_t sock_entry, struct task_struct_param *taskinfo);
void *read_files(paddr_t files_entry, struct task_struct_param *taskinfo);
paddr_t vaddr2paddr(paddr_t pgd_entry, vaddr_t vaddr);
void *hash_page(paddr_t paddr, uint32_t len, uint8_t *out_hash);
void *get_sha(paddr_t mm_entry, char *processName, struct task_struct_param *taskinfo,
	      struct pim_shaM *shaManagerClient);
TEE_Result put_sha(TEE_Param pParams[TEE_NUM_PARAMS], struct pim_shaM *shaManagerClient);
void *get_nsec_task_file_infos(struct task_struct_param *param);
void *get_nsec_task_mm_infos(struct task_struct_param *taskinfo, struct pim_shaM *shaManagerClient,
			     TEE_Param pParams[TEE_NUM_PARAMS]);
void *get_trusted_boot_result(struct pim_shaM *shaManagerClient, TEE_Param pParams[TEE_NUM_PARAMS]);
void *ta_api(enum ta_types type, struct task_struct_param *taskinfo, struct pim_shaM *shaManagerClient,
	     TEE_Param pParams[TEE_NUM_PARAMS]);
/* basic run-time tests */
TEE_Result get_task_info(uint32_t nParamTypes,
			 TEE_Param pParams[TEE_NUM_PARAMS]);

#endif /*GET_TASK_INFO_H*/
