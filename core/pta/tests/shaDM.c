//
//  shaDM.c
//  shaDM
//
//  Created by xuyiling on 07/02/2019.
//  Copyright © 2019 xuyiling. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <trace.h>
#include "shaDM.h"

static int mypower(int x, unsigned int y) {
	if (y == 0)
		return 1;
	else if ((y % 2) == 0)
		return mypower(x, y / 2) * mypower(x, y / 2);
	else
		return x * mypower(x, y / 2) * mypower(x, y / 2);
}

void clearn(struct pim_shaM *sham) {
	if (sham->isinit) {
		free(sham->shaStore);
		free(sham->procStore);
	}
	sham->isinit = 0;
}

void init(struct pim_shaM *sham, int mod) {
  unsigned int i = 0;

  sham->process_count = 8; //init 8 process, 1000
  sham->process_len = 0;
  sham->procStore = (struct pim_procStore *)malloc(sizeof(struct pim_procStore) * (sham->process_count));
  memset(sham->procStore, 0, sizeof(struct pim_procStore) * (sham->process_count));
  for (i = 0; i < sham->process_count; i++)
    sham->procStore[i].nameLen = -1;

  if (mod == 0) //server
  {
    sham->shaStore = (struct pim_shaStore *)malloc(sizeof(struct pim_shaStore) * (sham->volume));
    memset(sham->shaStore, 0, sizeof(struct pim_shaStore) * (sham->volume));
    for (i = 0; i < sham->volume; i++)
      sham->shaStore[i].processIndex = -1;
  }
  else
  {
    sham->sha_count = 48; //init 48 shas, 110000
    sham->sha_len = 0;
    sham->shaStore = (struct pim_shaStore *)malloc(sizeof(struct pim_shaStore) * (sham->sha_count));
    memset(sham->shaStore, 0, sizeof(struct pim_shaStore) * (sham->sha_count));
    for (i = 0; i < sham->sha_count; i++)
      sham->shaStore[i].processIndex = -1;
  }

  sham->isinit = 1;
}

unsigned int getValue(char *shaValue) // int str to hex value
{
  //here simplify the way
  unsigned int i, value = 0, tmp;
  char m;
  for (i = 0; i < 8; i++)
  {
    tmp = 0;
    m = *(shaValue + i);
    switch (m)
    {
    case '0':
      tmp = 0;
      break;
    case '1':
      tmp = 1;
      break;
    case '2':
      tmp = 2;
      break;
    case '3':
      tmp = 3;
      break;
    case '4':
      tmp = 4;
      break;
    case '5':
      tmp = 5;
      break;
    case '6':
      tmp = 6;
      break;
    case '7':
      tmp = 7;
      break;
    case '8':
      tmp = 8;
      break;
    case '9':
      tmp = 9;
      break;
    case 'a':
      tmp = 10;
      break;
    case 'b':
      tmp = 11;
      break;
    case 'c':
      tmp = 12;
      break;
    case 'd':
      tmp = 13;
      break;
    case 'e':
      tmp = 14;
      break;
    case 'f':
      tmp = 15;
      break;
    default:
      break;
    }
    value = (value << 4) + tmp;
  }
  return value;
}

// add the sha for one page of a certain process
void addSha(struct pim_shaM *sham, char *processName, char *shaValue)
{
  unsigned int p, pp;
  int tmp;
  struct pim_procStore *tmpProc;
  struct pim_shaStore *tmpSha;
  if (sham->isinit == 0)
    sham->init(sham, sham->Mode);

  if (sham->process_len >= sham->process_count) // extend procStore capacity +10
  {
    int oldsize1 = sham->process_count;
    sham->process_count += 10;
    tmpProc = sham->procStore;
    sham->procStore = (struct pim_procStore *)malloc(sizeof(struct pim_procStore) * (sham->process_count));
    memset(sham->procStore, 0, sizeof(struct pim_procStore) * (sham->process_count));
    memcpy(sham->procStore, tmpProc, sizeof(struct pim_procStore) * oldsize1);
    free(tmpProc);
  }

  for (p = 0; p < sham->process_len; p++)
    if (!strcmp(sham->procStore[p].name, processName))
      break;
  if (p >= sham->process_len)
  {
    p = sham->process_len;
    if (strlen(processName) > 64)
      return;
    memcpy(sham->procStore[p].name, processName, strlen(processName) + 1); // the last NULL
    sham->procStore[p].nameLen = strlen(processName);
    sham->process_len += 1;
  }

  if (sham->Mode == 1) //client
  {
    if (sham->sha_len >= sham->sha_count)
    {
      int oldsize1 = sham->sha_count;
      sham->sha_count += 100; // extend sha cpacity +100
      tmpSha = sham->shaStore;
      sham->shaStore = (struct pim_shaStore *)malloc(sizeof(struct pim_shaStore) * (sham->sha_count));
      memset(sham->shaStore, 0, sizeof(struct pim_shaStore) * (sham->sha_count));
      memcpy(sham->shaStore, tmpSha, sizeof(struct pim_shaStore) * oldsize1);
      free(tmpSha);
    }

    memcpy(sham->shaStore[sham->sha_len].shaValue, shaValue, strlen(shaValue) + 1);
    sham->shaStore[sham->sha_len].processIndex = p;
    sham->sha_len += 1;
    return;
  }
  else //server
  {
    pp = (getValue(shaValue)) % sham->module;
    tmp = sham->volume;
    while (sham->shaStore[pp % sham->module].processIndex >= 0 && tmp >= 0)
    {
      tmp--;
      pp++;
    }
    memcpy(sham->shaStore[pp % sham->module].shaValue, shaValue, strlen(shaValue) + 1);
    sham->shaStore[pp % sham->module].processIndex = p;
  }
}

unsigned int getShaMLen(struct pim_shaM *sham)
{
  unsigned int len = 0;
  if (sham->Mode == 1)                                       //client
    len = sham->process_len * 65 + sham->sha_len * (65 + 5); //process_len * 65：name的空间；sham->sha_len * 70：shaValue+ index的空间
  else
    len = sham->process_len * 65 + sham->volume * (65 + 5);
  len += 6 + 10 + 10 + 20 + 5 + 10; //

  return len + 1; //\0
}

/**
 * shift between int and str
 * 
 * @param: isInt2Str  direction
 * @param: num        number of an int, for example, 3 for 132, 4 for 6754
*/
void dataShift(int isInt2Str, int *data, char *str, int num)
{
  int i, tmp = num, v = 0;
  if (isInt2Str) // int 2 str
  {
    if (*data < 0)
    {
      for (i = 0; i < num; i++)
	str[i] = 'F';
      return;
    }
    while (tmp > 0)
    {
      switch (tmp)
      {
      case 10:
	v = (int)(*data / mypower(10, 9)) % 10;
	break;
      case 9:
	v = (int)(*data / mypower(10, 8)) % 10;
	break;
      case 8:
	v = (int)(*data / mypower(10, 7)) % 10;
	break;
      case 7:
	v = (int)(*data / mypower(10, 6)) % 10;
	break;
      case 6:
	v = (int)(*data / mypower(10, 5)) % 10;
	break;
      case 5:
	v = (int)(*data / mypower(10, 4)) % 10;
	break;
      case 4:
	v = (int)(*data / mypower(10, 3)) % 10;
	break;
      case 3:
	v = (int)(*data / mypower(10, 2)) % 10;
	break;
      case 2:
	v = (int)(*data / mypower(10, 1)) % 10;
	break;
      case 1:
	v = (int)(*data / mypower(10, 0)) % 10;
	break;
      default:
	break;
      }
      str[num - tmp] = v + '0';
      tmp--;
    }
    str[num] = 0;
  }
  else // str to int
  {
    *data = 0;
    if (str[0] == 'F')
    {
      *data = -1;
      return;
    }
    for (i = 0; i < num; i++)
    {
      switch (str[i])
      {
      case '0':
	v = 0;
	break;
      case '1':
	v = 1;
	break;
      case '2':
	v = 2;
	break;
      case '3':
	v = 3;
	break;
      case '4':
	v = 4;
	break;
      case '5':
	v = 5;
	break;
      case '6':
	v = 6;
	break;
      case '7':
	v = 7;
	break;
      case '8':
	v = 8;
	break;
      case '9':
	v = 9;
	break;
      default:
	break;
      }
      *data = *data * 10 + v;
    }
  }
}

//parttern:  #![maxlen(9+,)][nonce(10)][volum(9+,)][mod(9+,)][procNum(4+,)][processname1+,][processname2+,]...##[shaNum(9+,)][shaValue1+,][index1(4+,)][shaValue2+,][index2(4+,)]...#$
//namely:#![#?] + maxLen[10]+ nonce[10] + volum[10] + mod[10] + procNum[5] + procList + ## + shaNum[10] + shaList[index=5] +#$
//','as '\0'
void getPack(struct pim_shaM *sham, uint8_t *nonce, char *target)
{
  unsigned i, j, m, p = 0, cnt = 0;
  int len, ispad;
  char tmp[15] = {0};

  target[0] = '#';
  if (sham->Mode == 1)
    target[1] = '!';
  else
    target[1] = '?';

  len = getShaMLen(sham);
  dataShift(1, &len, tmp, 9);
  for (p = 2, i = 0; i < 9; i++, p++)
    target[p] = tmp[i];
  target[p] = ',';
  p++; //p=12

  for (i = 0; i < 10; i++, p++)
    target[p] = nonce[i];

  if (sham->Mode == 0) //server
  {
    len = sham->volume;
    dataShift(1, &len, tmp, 9);
    for (i = 0; i < 9; i++, p++)
      target[p] = tmp[i];
    target[p] = ',';
    p++;

    len = sham->module;
    dataShift(1, &len, tmp, 9);
    for (i = 0; i < 9; i++, p++)
      target[p] = tmp[i];
    target[p] = ',';
    p++;
  }
  else
  {
    for (i = 0; i < 20; i++, p++) // empty for client payload
      target[p] = ',';
  }

  len = sham->process_len;
  dataShift(1, &len, tmp, 4);
  for (i = 0; i < 4; i++, p++)
    target[p] = tmp[i];
  target[p] = ',';
  p++;

  for (i = 0; i < sham->process_len; i++)
  {
    ispad = 0;
    for (j = 0; j < 65; j++, p++) //cpy 33 process name
    {
      if (sham->procStore[i].name[j] == 0)
	ispad = 1;
      if (ispad)
	target[p] = ',';
      else
	target[p] = sham->procStore[i].name[j];
    }
  }

  target[p] = '#';
  p++;
  target[p] = '#';
  p++;

  if (sham->Mode == 1) //client
  {
    len = sham->sha_len;
    cnt = sham->sha_len;
  }
  else
  {
    len = sham->volume;
    cnt = sham->volume;
  }
  dataShift(1, &len, tmp, 9); //sha Count
  for (i = 0; i < 9; i++, p++)
    target[p] = tmp[i];
  target[p] = ',';
  p++;

  for (i = 0; i < cnt; i++)
  {
    ispad = 0;
    for (j = 0; j < 65; j++, p++)
    {
      if (sham->shaStore[i].shaValue[j] == 0)
	ispad = 1;
      if (ispad)
	target[p] = ',';
      else
	target[p] = sham->shaStore[i].shaValue[j];
    }

    len = sham->shaStore[i].processIndex; //process Index
    dataShift(1, &len, tmp, 4);
    for (m = 0; m < 4; m++, p++)
      target[p] = tmp[m];
    target[p] = ',';
    p++;
  }
  target[p] = '#';
  p++;
  target[p] = '$';
  p++;
  target[p] = 0;
}
