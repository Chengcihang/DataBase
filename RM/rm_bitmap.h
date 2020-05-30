//  rm_bitmap.h
//  DataBaseSystem
//  位图的数据结构以及相关操作
//  Created by CihangCheng on 2020/3/30.
//  Copyright © 2020 社区风险项目. All rights reserved.
//
#ifndef DATABASE_RM_BITMAP_H
#define DATABASE_RM_BITMAP_H
#include "../redbase.h"
/**
 * @brief 位图类，是记录在页中存储情况的映射
 *        由于对位图需要一些操作，在此抽象成一个类
 *        RM_BitMap的对象在栈中，char* bitmap指向内存
 */
class RM_BitMap{
public:
    RM_BitMap   ();
    RM_BitMap   (char *_bitmap, int bitmapCount);                // 主要的构造函数
    ~RM_BitMap  ();
    char * GetBitMap () const;  // 得到位图的地址
    int BitMapCount  () const;  // 位图使用的二进制位数
    int BitMapSize   () const;  // 位图占用的字节数

    RC SetBitMapCount(int count);// 设置位图的字节数

    RC Reset            ();                                     // 位图所有位设置为0
    RC SetBit           (int bit);                              // 将位图某一位设置为1，插入操作
    RC ResetBit         (int bit);                              // 将位图某一位设置为0，删除操作
    Boolean CheckBit    (int bitNum) const;                     // 判断bitNum位的值是否为1

    int GetFirstZeroBit () const ;                              // 得到位图的第一个0位的槽号，查找插入
    int GetNextOneBit   (int start) const;                      // 从start位开始，得到下一个1的槽号，记录查询

    static int CountToLen(int count);                           // 根据位数计算占用字节数

private:
    char* bitmap;   // 由于不知道位图具体用到多少位，所以用指针来表示
    int bitmapLen;  // 位图占用的字节数
    int bitCount;   // 位图占用的二进制位数，也就是一个页的可用记录数
};

#endif //DATABASE_RM_BITMAP_H
