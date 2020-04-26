//  rm_bitmap.cc
//  DataBaseSystem
//  位图类的实现
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#include "../Header/rm_bitmap.h"
#include "../Header/rm.h"

/**
 * @brief 构造函数，其实没用
 *        BitMap是指向内存的
 */
RM_BitMap::RM_BitMap() {
    this->bitmapLen = 0;
    this->bitmapLen = NULL;
    this->bitCount = 0;
}

/**
 * @brief 析构函数，也没用，因为不会实例化
 */
RM_BitMap::~RM_BitMap()=default;

/**
 * @brief 返回该bitmap在缓冲区中的数据地址
 * @return 缓冲区中的bitmap字符串
 */
char *RM_BitMap::GetBitMap() const {
    return this->bitmap;
}

/**
 * @brief 返回bitmap占用的字节数
 * @return
 */
int RM_BitMap::BitMapCount() const {
    return this->bitCount;
}

/**
 * 将bitmap的所有位都设置为0
 * @return OK_RC
 */
RC RM_BitMap::Reset() {
    for(int i=0; i < this->bitmapLen; i++)
        bitmap[i] = bitmap[i] ^ bitmap[i];
    return OK_RC;
}

/**
 * 将bit位的二进制位设置为1
 * @param bit
 * @return
 */
RC RM_BitMap::SetBit(int bit) {
    // 修改位超过限制
    if(bit>this->bitCount-1)
        return RM_INVALIDBITOPERATION;
    // 字节位
    int index = bit / 8;
    // 偏移量
    int offset = bit % 8;
    this->bitmap[index] |= (0x01 << offset);
    return OK_RC;
}

/**
 * 将bit位的二进制位设置为0
 * @param bit
 * @return
 */
RC RM_BitMap::ResetBit(int bit) {
    // 修改位超过限制
    if(bit>this->bitCount-1)
        return RM_INVALIDBITOPERATION;
    // 字节位
    int index = bit / 8;
    // 偏移量
    int offset = bit % 8;
    this->bitmap[index] &= ~(0x01 << offset);
    return OK_RC;
}

Boolean RM_BitMap::CheckBit(int bitNum) const {
    // 位超过限制
    if(bitNum>this->bitCount-1)
        return RM_INVALIDBITOPERATION;
    // 字节位
    int index = bitNum / 8;
    // 偏移量
    int offset = bitNum % 8;
    if ((this->bitmap[index] & (1 << offset)) != 0)
        return TRUE;
    else
        return FALSE;
}

/**
 * 返回位图中首个0位的位号
 * 如果所有位都为1,返回-1
 * @return 槽号  首个0位
 *         -1   全为1
 */
int RM_BitMap::GetFirstZeroBit() const {
    // 按位遍历
    for(int bit = 0;bit < this->bitCount;++bit){
        int index = bit / 8;
        int offset = bit % 8;
        if((bitmap[index] & (1 << offset)) == 0)
            return bit;
    }
    // 没有一位为0
    return -1;
}

/**
 * 返回位图中从start位开始的首个1位的位号
 * 如果所有位都为0,返回-1
 * @return 槽号  首个1位
 *         -1   全为0,或者start不合法
 */
int RM_BitMap::GetNextOneBit(int start) const {
    if (start < 0 || start >= this->bitCount)
        return -1;

    // 按位遍历
    for(int bit = start;bit < this->bitCount;++bit){
        int index = bit / 8;
        int offset = bit % 8;
        if((bitmap[index] & (1 << offset)) != 0)
            return bit;
    }
    return -1;
}

/**
 * (位数+1)/8
 * @param count 位数
 * @return 字节数
 */
int RM_BitMap::CountToLen(int count) {
    int bitmapSize = (count + 1) / 8;
    return bitmapSize;
}

int RM_BitMap::BitMapSize() const {
    return this->bitmapLen;
}

RC RM_BitMap::SetBitMapCount(int count) {
    this->bitCount = count;
    // 重置字节数
    this->bitmapLen = CountToLen(this->bitCount);
    return OK_RC;
}

/**
 * 根据_bitmap指向的缓冲区构造位图对象
 * @param _bitmap
 * @param bitmapCount
 */
RM_BitMap::RM_BitMap(char *_bitmap, int bitmapCount) {
    this->bitmap = bitmap;
    this->bitCount = bitmapCount;
    this->bitmapLen = CountToLen(this->bitCount);
}
