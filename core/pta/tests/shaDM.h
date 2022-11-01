//
//  shaDM.h
//  shaDM
//  Created by xuyiling on 07/02/2019.
//  Copyright © 2019 xuyiling. All rights reserved.
//

#include <utee_defines.h>

#ifndef SHADM_H
#define SHADM_H

struct pim_shaStore {
	char shaValue[65];
	int processIndex;
};

struct pim_procStore {
	char name[65];
	int nameLen;
};

/*
 * if two sha value are equal, that case maybe wrong, need checked
 * here are some restrict
 * 1.set the number of processes is from 0 to 9999, 假设客户端总进程<9999
 * 2.set the max processes' length is 64
 */

struct pim_shaM {
	int Mode;   //working mode, work at server when set 0, work at client when set other value.
	int isinit; //if set 0, you need to set variable 'volume' and 'module' , and make sure volume>module

	unsigned int volume; //服务端用，当前hash值的最大容量值
	unsigned int module;

	struct pim_shaStore *shaStore;
	unsigned int sha_len;
	unsigned int sha_count;

	struct pim_procStore *procStore;
	unsigned int process_len;   //当前进程结构体已经占用的个数
	unsigned int process_count; //当前结构体的容量

	void (*init)(struct pim_shaM *cur, int mod);                             // init the client or server server,if mod==0, please set variable 'volume' and 'module' first.
	void (*addSha)(struct pim_shaM *cur, char *processName, char *shaValue); //add a sha value

	void (*getPack)(struct pim_shaM *cur, uint8_t *nonce, char *target);          //把pim_shaM转成字符串
	unsigned int (*getShaMLen)(struct pim_shaM *cur);                             //把pim_shaM转成字符串后，计算字符串长度
	void (*unPack)(struct pim_shaM *cur, char *target, struct pim_shaM *tmp_shaM); //把字符串转成pim_shaM
	int (*matchPack)(struct pim_shaM *cur, char *target, char *result);           //服务器端代码，两个字符串进行比对
	int (*find)(struct pim_shaM *cur, char *processName, char *shaValue);         //两边都用，查找结构体是否存储sha值
	int (*getPackShaNum)(char *target);                                          //获得字符串里面sha值的个数

	void (*clearn)(struct pim_shaM *cur);
	int (*readFromFile)(struct pim_shaM *cur, char *path);
	int (*readFromString)(struct pim_shaM *cur, char *path);
};

void init(struct pim_shaM *sham, int mod);
unsigned int getValue(char *shaValue);
void addSha(struct pim_shaM *sham, char *processName, char *shaValue);
void clearn(struct pim_shaM *sham);
unsigned int getShaMLen(struct pim_shaM *sham);
void dataShift(int isInt2Str, int *data, char *str, int num);
void getPack(struct pim_shaM *sham, uint8_t *nonce, char *target);
void unPack(struct pim_shaM *sham, char *target, struct pim_shaM *tmp_shaM);
int matchPack(struct pim_shaM *sham, char *target, char *result);
int find(struct pim_shaM *sham, char *processName, char *shaValue);
int readFromFile(struct pim_shaM *sham, char *path);
int readFromString(struct pim_shaM *sham, char *path);
int getPackShaNum(char *target);

#endif /* SHADM_H */
