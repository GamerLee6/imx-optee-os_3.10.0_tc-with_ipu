#include <kernel/panic.h>
#include <kernel/misc.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <mm/mobj.h>
#include <tee/tee_cryp_utl.h>
#include <tee/crypt_sha.h>
#include <string.h>
#include <util.h>
#include <assert.h>
#include <malloc.h>
#include <stdbool.h>
#include <trace.h>
#include <utee_defines.h>

#include "get_task_info.h"

static bool pim_lock = false;
static int initmem = 0;
const unsigned char Num2CharTable[] = "0123456789abcdef";

static void init_mem(size_t len)
{
	size_t regionsize;

	regionsize = (len & ~SMALL_PAGE_MASK) + SMALL_PAGE_SIZE;
	if (pim_core_mmu_add_mapping(MEM_AREA_RAM_SEC, regionsize))
		DMSG("Add private mm region success");
}

/**
 * Linux task_struct mm file semantics
 * The initialized info is unique to every system.
*/
static void init_taskparam(struct task_struct_param *taskinfo) {
	taskinfo->init_task = 0x11007300;
	taskinfo->va2pa_offset = 0x70000000;
	taskinfo->pid_offset = 768;
	taskinfo->comm_offset = 1028;
	taskinfo->tasks_offset = 596; // struct list_head tasks;
	taskinfo->files_offset = 1064;
	taskinfo->fdt_offset = 20;
	taskinfo->fd_array_offset = 84;
	taskinfo->max_fds_offset = 0;
	taskinfo->fd_offset = 4;
	taskinfo->f_path_offset = 8;
	taskinfo->private_data_offset = 144;
	taskinfo->dentry_offset = 4;
	taskinfo->d_parent_offset = 16;
	taskinfo->d_iname_offset = 44;
	taskinfo->d_name_offset = 24;
	taskinfo->name_offset = 8;
	taskinfo->type_offset = 4;
	taskinfo->sk_offset = 20;
	taskinfo->sk_common_offset = 0;
	taskinfo->skc_daddr_offset = 0;
	taskinfo->skc_rcv_saddr_offset = 4;
	taskinfo->skc_v6_daddr_offset = 40;
	taskinfo->skc_v6_rcv_saddr_offset = 56;
	taskinfo->skc_dport_offset = 12;
	taskinfo->skc_num_offset = 14;

	taskinfo->mm_offset = 636;
	taskinfo->pgd_offset = 36;
	taskinfo->scode_offset = 128;
	taskinfo->ecode_offset = 132;
}

static void init_shaparam(struct pim_shaM *shaManagerClient) {
	shaManagerClient->Mode = 1;   // client
	shaManagerClient->isinit = 0; // uninitialized
	shaManagerClient->init = init;
	shaManagerClient->addSha = addSha;
	shaManagerClient->getPack = getPack;
	shaManagerClient->getShaMLen = getShaMLen;
	shaManagerClient->clearn = clearn;
}

void HexArrayToString(char *string, uint8_t *hexarray, int length) {
	int i = 0;
	while (i < length)
	{
		*(string++) = Num2CharTable[((hexarray[i] >> 4) & 0x0f)];
		*(string++) = Num2CharTable[(hexarray[i] & 0x0f)];
		i++;
	}
	*string = 0x0;
}

void pathcat(char *dest, char *src) {
	char *tmp;
	int dlen = strlen(dest);
	int slen = strlen(src);
	if (strcmp(src, "/") != 0) {
		tmp = (char *)malloc(sizeof(char) * (dlen + slen + 2));
		memset(tmp, 0, sizeof(char) * (dlen + slen + 2));
		tmp[0] = '/';
		memcpy(tmp + 1, src, slen);
		memcpy(tmp + slen + 1, dest, dlen);
		tmp[dlen + slen + 1] = '\0';
		memcpy(dest, tmp, dlen + slen + 2);
		free(tmp);
	}
}

paddr_t vaddr2paddr(paddr_t pgd_entry, vaddr_t vaddr) {
	/* TEE's page tranversal to find the paddr of 
	 * the corresponding linux user-space vm addr */ 
	paddr_t pgd_index;
	paddr_t pte_index;
	paddr_t paddr;
	struct tee_mmap_region map;

	pgd_index = (pgd_entry & PAGE_MASK) 
			+ sizeof(paddr_t) * pgd_index(vaddr);
	pim_region_mapping(pgd_index, 0x2000, &map); //map pgd_index
	if (((*(vaddr_t *)map.va) & 1) != 1)        //page不在内存中
		return 0;

	pte_index = ((*(paddr_t *)map.va) & PAGE_MASK) 
			+ sizeof(paddr_t) * pte_index(vaddr);
	pim_region_mapping(pte_index, 0x2000, &map); //map pte_index
	if (((*(vaddr_t *)map.va) & 1) != 1)        //page不在内存中
		return 0;

	paddr = ((*(paddr_t *)map.va) & PAGE_MASK) | (vaddr & ~PAGE_MASK);
	return paddr;
}

void *hash_page(paddr_t paddr, uint32_t len, uint8_t *out_hash) {
	struct tee_mmap_region map;

	pim_region_mapping(paddr, 0x2000, &map);
	tee_hash_createdigest(TEE_ALG_SHA256, (uint8_t *)map.va, 
				len, out_hash, TEE_SHA256_HASH_SIZE);

	return NULL;
}

void *get_sha(paddr_t mm_entry, char *processName, struct task_struct_param *taskinfo,
	      struct pim_shaM *shaManagerClient) {
	uint32_t va2pa_offset = taskinfo->va2pa_offset;
	uint32_t pgd_offset = taskinfo->pgd_offset;
	uint32_t scode_offset = taskinfo->scode_offset;
	uint32_t ecode_offset = taskinfo->ecode_offset;
	struct tee_mmap_region map;
	paddr_t pgd_entry;
	paddr_t paddr;
	vaddr_t scode, ecode;
	uint32_t pagecount;
	uint32_t pagesize;
	uint8_t out_hash[TEE_SHA256_HASH_SIZE];
	char name[strlen(processName) + 1];
	char out_hash_str[TEE_SHA256_HASH_SIZE * 2 + 1];

	memcpy(name, processName, strlen(processName));
	name[strlen(processName)] = '\0';
        
	pim_region_mapping(mm_entry, 0x2000, &map); // map mm_struct
	pgd_entry = *(vaddr_t *)(map.va + pgd_offset) - va2pa_offset; // paddr of pgd
	scode = *(vaddr_t *)(map.va + scode_offset); // vaddr of start_code
	ecode = *(vaddr_t *)(map.va + ecode_offset); // vaddr of end_code
	pagecount = ((ecode - scode) >> SMALL_PAGE_SHIFT) + 1;
	pagecount = pagecount > MAX_PAGE_COUNT ? MAX_PAGE_COUNT : pagecount; // the max pagecount is set to 198
	pagesize = (ecode - scode) > SMALL_PAGE_SIZE ? SMALL_PAGE_SIZE : (ecode - scode);

	do{
		paddr = vaddr2paddr(pgd_entry, scode); // paddr of a code page
		// page in memory
		if (paddr != 0)	{
			hash_page(paddr, pagesize, out_hash);
			HexArrayToString(out_hash_str, out_hash, TEE_SHA256_HASH_SIZE);
                        DMSG("%s, %s", name, out_hash_str);
			shaManagerClient->addSha(shaManagerClient, name, out_hash_str);
		}
		scode = scode + pagesize; // manipulate on the linux user-space vaddr
		pagesize = (ecode - scode) > SMALL_PAGE_SIZE ? SMALL_PAGE_SIZE : (ecode - scode);
		pagecount--;
	} while (pagecount != 0);

	return NULL;
}

//package: #! + package_sz[9+,] + encryped(mm) + #$
TEE_Result put_sha(TEE_Param pParams[TEE_NUM_PARAMS], struct pim_shaM *shaManagerClient) {
	int len, cipher_sz, package_sz;
	char package_sz_str[9] = {0};
	char *mm;
	uint8_t *nonce;
	uint8_t *cipher = pParams[0].memref.buffer;

	nonce = pParams[0].memref.buffer;

	len = shaManagerClient->getShaMLen(shaManagerClient);
	cipher_sz = (len / TEE_AES_BLOCK_SIZE + 1) * TEE_AES_BLOCK_SIZE;
	package_sz = 2 + 10 + cipher_sz + 2;
	dataShift(1, &package_sz, package_sz_str, 9);

	mm = (char *)calloc(cipher_sz, sizeof(char));
	if (!mm) {
		IMSG("error in memory alloc");
		return TEE_ERROR_OUT_OF_MEMORY;
	}
	shaManagerClient->getPack(shaManagerClient, nonce, mm);
	tee_aes_cbc_crypt((uint8_t *)mm, cipher + 12, cipher_sz, nonce, TEE_MODE_ENCRYPT);

	memcpy(cipher, "#!", 2);
	memcpy(cipher + 2, package_sz_str, 9);
	cipher[2 + 9] = ',';
	memcpy(cipher + 12 + cipher_sz, "#$", 2);
	cipher[12 + cipher_sz + 2] = 0;

	free(mm);
	return 0;
}

/*
init
syslogd
klogd
udhcpc
sh
tee-supplicant
notifier
ffmpeg
*/
const char *proc_white_list[] = {
        "autossh",
        "ssh",
        "sleep",
        "sshd",
        "getty",
        "frpc",
        "echo",
        "sha512sum",
        "cut",
        "cp",
        "rm",
};

const int proc_white_list_size = 
	sizeof(proc_white_list) / sizeof(proc_white_list[0]);


int is_proc_in_wl(char *comm) {
        int res = 0;

        if (comm == NULL)
                return 1;

        for (size_t i = 0; i < proc_white_list_size; i++) {
                if (strcmp(comm, proc_white_list[i]) == 0) {
                        res = 1;
                        break;
                }
        }

        return res;
}

int init_count = 0;

void *get_nsec_task_mm_infos(struct task_struct_param *taskinfo, 
			struct pim_shaM *shaManagerClient, TEE_Param pParams[TEE_NUM_PARAMS]) {
	paddr_t task_entry = pParams[1].value.b; // task_struct paddr for init
	uint32_t va2pa_offset = taskinfo->va2pa_offset;
	uint32_t pid_offset = taskinfo->pid_offset;
	uint32_t comm_offset = taskinfo->comm_offset;
	uint32_t tasks_offset = taskinfo->tasks_offset;
	uint32_t mm_offset = taskinfo->mm_offset;
	uint32_t pid;
	char *comm;
	paddr_t mm_entry;
	struct tee_mmap_region map;
	do {
		pim_region_mapping(task_entry, 0x2000, &map); // map task_struct into TEE vm space
		pid = *(uint32_t *)(map.va + pid_offset);
		comm = (char *)(map.va + comm_offset);

                //break the loop by force
                if (strcmp(comm, "init") == 0) {
                        init_count++;
                }

                if (init_count >= 2) {
                        init_count = 0;
                        pParams[1].value.b = taskinfo->init_task;
                        return NULL;
                }


		// paddr of next task_struct in the task list
		task_entry = *(vaddr_t *)(map.va + tasks_offset) - tasks_offset - va2pa_offset;
		// paddr of current task_struct mm
		mm_entry = *(vaddr_t *)(map.va + mm_offset) - va2pa_offset; 
	} while ((*(vaddr_t *)(map.va + mm_offset) == 0 && task_entry != taskinfo->init_task) 
                || is_proc_in_wl(comm));
	// until find a process, or at the end of the tasklist

	// the last task_struct maybe a thread
	if (*(vaddr_t *)(map.va + mm_offset) != 0) {
		DMSG("pid %d (%s)", pid, comm);
		get_sha(mm_entry, comm, taskinfo, shaManagerClient);
		put_sha(pParams, shaManagerClient);
	}
	else
		memset(pParams[0].memref.buffer, 0, pParams[0].memref.size);

	pParams[1].value.b = (uint32_t)task_entry; // tell the ca who's' next

	return NULL;
}

extern uint8_t rootfs_hash[];
void *get_trusted_boot_result(struct pim_shaM *shaManagerClient, 
				TEE_Param pParams[TEE_NUM_PARAMS]){
	return NULL;
}

void *ta_api(enum ta_types type, struct task_struct_param *taskinfo, 
		struct pim_shaM *shaManagerClient,
		TEE_Param pParams[TEE_NUM_PARAMS]) {
	void *ret = NULL;

	switch (type) {
		case GET_TRUSTED_BOOT_RESULT:
			ret = get_trusted_boot_result(shaManagerClient, pParams);
			break;
		case PRINT_NSEC_TASK_MM_INFOS:
			ret = get_nsec_task_mm_infos(taskinfo, shaManagerClient, pParams);
			break;
		default:
			ret = NULL;
			break;
	}

	return ret;
}

TEE_Result get_task_info(uint32_t nParamTypes __unused,
			 TEE_Param pParams[TEE_NUM_PARAMS] __unused) {
	struct task_struct_param taskinfo;
	struct pim_shaM shaManagerClient;

	init_taskparam(&taskinfo);
	init_shaparam(&shaManagerClient);

	if (initmem == 0) {
		init_mem(0x400000);
		initmem = 1;
	}

	shaManagerClient.init(&shaManagerClient, shaManagerClient.Mode);
	// init at first time
	if (pParams[1].value.a == 0) {
		ta_api(GET_TRUSTED_BOOT_RESULT, &taskinfo, &shaManagerClient, pParams);
	} else {
		ta_api(PRINT_NSEC_TASK_MM_INFOS, &taskinfo, &shaManagerClient, pParams);
	}

	shaManagerClient.clearn(&shaManagerClient);

	return TEE_SUCCESS;
}
