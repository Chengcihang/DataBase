//
//  Printer.cpp
//  MicroDBMS
//
//  Created by 全俊源 on 2020/4/27.
//  Copyright © 2020 社区风险项目. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include "printer.h"

using namespace std;

//构造函数，元组的打印
Printer::Printer(const DataAttrInfo *attributes_,
                 const int attrCount_) {
    attrCount = attrCount_;
    attributes = new DataAttrInfo[attrCount];

    for (int i=0; i < attrCount; i++)
        attributes[i] = attributes_[i];

    //打印的元组数
    iCount = 0;

    //标题信息
    psHeader = (char**)malloc(attrCount * sizeof(char*));

    //每个属性之间的空格数
    spaces = new int[attrCount];

    //需要打印的属性数
    for (int i = 0; i < attrCount; i++) {
        //尝试在另一列中查找属性
        int bFound = 0;
        psHeader[i] = new char[MAXPRINTSTRING];
        memset(psHeader[i],0,MAXPRINTSTRING);

        for (int j = 0; j < attrCount; j++)
            if (j != i &&
                strcmp(attributes[i].attrName,
                       attributes[j].attrName) == 0) {
                bFound = 1;
                break;
            }

        if (bFound)
            sprintf(psHeader[i], "%s.%s",
                    attributes[i].relName, attributes[i].attrName);
        else
            strcpy(psHeader[i], attributes[i].attrName);

        if (attributes[i].attrType == STRING)
            spaces[i] = min(attributes[i].attrLength, MAXPRINTSTRING);
        else
            spaces[i] = max(12, strlen(psHeader[i]));

        //减去标头
        spaces[i] -= strlen(psHeader[i]);

        //如果有负数（或零）空格，则插入一个
        if (spaces[i] < 1) {
            spaces[i] = 0;
            strcat(psHeader[i]," ");
        }
    }
}


//析构函数
Printer::~Printer() {
    for (int i = 0; i < attrCount; i++)
        delete [] psHeader[i];

    delete [] spaces;
    //删除psHeader
    free (psHeader);
    delete [] attributes;
}


//输出一些空格，以便打印条目将所有内容对齐
void Spaces(int maxLength,
            int printedSoFar) {
    for (int i = printedSoFar; i < maxLength; i++)
        cout << " ";
}


//打印header
void Printer::PrintHeader( ostream &c ) const {
    int dashes = 0;
    int iLen;
    int i, j;

    for (i = 0; i < attrCount; i++) {
        //打印标题信息名称
        c << psHeader[i];
        iLen = strlen(psHeader[i]);
        dashes += iLen;

        for (j = 0; j < spaces[i]; j++)
            c << " ";

        dashes += spaces[i];
    }

    c << "\n";
    for (i = 0; i < dashes; i++) c << "-";
    c << "\n";
}


//打印footer
void Printer::PrintFooter(ostream &c) const {
    c << "\n";
    c << iCount << " tuple(s).\n";
}


//打印函数 样式一
void Printer::Print(ostream &c,
                    const void * const data[])
{
    char str[MAXPRINTSTRING], strSpace[50];
    int i, a;
    float b;

    //增加打印的元组数
    iCount++;

    for (i = 0; i<attrCount; i++) {
        if (attributes[i].attrType == STRING) {
            //打印出前MAXNAME + 10个字符
            memset(str,0,MAXPRINTSTRING);

            if (attributes[i].attrLength > MAXPRINTSTRING) {
                strncpy(str, (char *)data[i], MAXPRINTSTRING-1);
                str[MAXPRINTSTRING-3] ='.';
                str[MAXPRINTSTRING-2] ='.';
                c << str;
                Spaces(MAXPRINTSTRING, strlen(str));
            } else {
                strncpy(str, (char *)data[i], attributes[i].attrLength);
                c << str;
                if (attributes[i].attrLength < (int) strlen(psHeader[i]))
                    Spaces(strlen(psHeader[i]), strlen(str));
                else
                    Spaces(attributes[i].attrLength, strlen(str));
            }
        }
        if (attributes[i].attrType == INT) {
            memcpy (&a, data[i], sizeof(int));
            sprintf(strSpace, "%d",a);
            c << a;
            if (strlen(psHeader[i]) < 12)
                Spaces(12, strlen(strSpace));
            else
                Spaces(strlen(psHeader[i]), strlen(strSpace));
        }
        if (attributes[i].attrType == FLOAT) {
            memcpy (&b, data[i], sizeof(float));
            sprintf(strSpace, "%f",b);
            c << strSpace;
            if (strlen(psHeader[i]) < 12)
                Spaces(12, strlen(strSpace));
            else
                Spaces(strlen(psHeader[i]), strlen(strSpace));
        }
    }
    c << "\n";
}

//打印函数 样式二
void Printer::Print(ostream &c, const char * const data)
{
    char str[MAXPRINTSTRING], strSpace[50];
    int i, a;
    float b;

    if (data == NULL)
        return;

    //增加打印的元组数
    iCount++;

    for (i = 0; i<attrCount; i++) {
        if (attributes[i].attrType == STRING) {
            //打印出前MAXNAME + 10个字符
            memset(str, 0, MAXPRINTSTRING);

            if (attributes[i].attrLength > MAXPRINTSTRING) {
                strncpy(str, data+attributes[i].offset, MAXPRINTSTRING-1);
                str[MAXPRINTSTRING-3] ='.';
                str[MAXPRINTSTRING-2] ='.';
                c << str;
                Spaces(MAXPRINTSTRING, strlen(str));
            } else {
                strncpy(str, data+attributes[i].offset, attributes[i].attrLength);
                c << str;
                if (attributes[i].attrLength < (int) strlen(psHeader[i]))
                    Spaces(strlen(psHeader[i]), strlen(str));
                else
                    Spaces(attributes[i].attrLength, strlen(str));
            }
        }
        if (attributes[i].attrType == INT) {
            memcpy (&a, (data+attributes[i].offset), sizeof(int));
            sprintf(strSpace, "%d",a);
            c << a;
            if (strlen(psHeader[i]) < 12)
                Spaces(12, strlen(strSpace));
            else
                Spaces(strlen(psHeader[i]), strlen(strSpace));
        }
        if (attributes[i].attrType == FLOAT) {
            memcpy (&b, (data+attributes[i].offset), sizeof(float));
            sprintf(strSpace, "%f",b);
            c << strSpace;
            if (strlen(psHeader[i]) < 12)
                Spaces(12, strlen(strSpace));
            else
                Spaces(strlen(psHeader[i]), strlen(strSpace));
        }
    }
    c << "\n";
}

