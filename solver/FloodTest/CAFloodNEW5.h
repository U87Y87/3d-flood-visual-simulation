#if !defined(AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_)
#define AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include<iostream>
#include <vector>
#include<sstream>
#include <fstream>
#include<iomanip>
#include<windows.h>
#include"CoordinateConversion.h"
using namespace std;

#define NODATA -9999
#define g 9.8
#define zero 0.00001
//*元胞单元结构*//
typedef struct CACell 
{
	bool b_IsRiver;//是否为河流区域
	bool b_IsOrigin;//是否为源
	bool b_IsEnd;//是否为尾
	double DemData;
	double Lat;//对应的经纬度
	double Long;//对应的经纬度
	double CoaseRate;//糙率
	double WaterHeight_New;//新的水高
	double WaterHeight_Old;//旧的水高
	double XUnitFlux_New; //x方向新的单宽流量(右邻格流到该格的单宽流量，负号表示是从该格往右邻格流)
	double YUnitFlux_New; //y方向新的单宽流量(上邻格流到该格的单宽流量，负号表示是从该格往上邻格流)
	double XUnitFlux_Old; //x方向旧的单宽流量
	double YUnitFlux_Old; //y方向旧的单宽流量
	double vec;	//绝对流速
}CACELL;

//*基流初始化用*//
typedef struct RiverInput
{	
	float RiverHight;
	float XUnitFlux;
	float YUnitFlux;
	long x;
	long y;
}RIVERINPUT;

//*河道索引用*//
typedef struct RiverIndex
{
	long x;
	long y;
}RIVERINDEX;

typedef struct DamInformation
{
	//溃口信息	
	bool m_bFullBreak; //是否全溃
	float m_fTypeFactor; 
	float m_fDamVolume; //坝的库容
	float m_fDamWidth;  //坝宽
	float m_fDamHeight; //坝高
	float m_fBreakWidth; //溃口宽度
	float m_fBreakHeight;//溃口高度
}DAMINFORMATION;

typedef struct DEMInformation
{
	int m_nWidth;
	int m_nHeight;
	float m_fCellsize;	
	float m_xllcorner;
	float m_yllcorner;
}DEMINFORMATION;

#endif // !defined(AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_)