#if !defined(AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_)
#define AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include<iostream>
#include <vector>
#include<sstream>
#include <fstream>
#include"CoordinateConversion.h"
using namespace std;
namespace FLOOD3
{
#define NODATA -9999
#define g 9.8
#define zero 0.00001

//*元胞单元结构*//
typedef struct CACell 
{
	bool b_IsRiver;//是否为河流区域
	bool b_IsOrigin;//是否为源
	bool b_IsEnd;//是否为尾
	float DemData;
	float Lat;//对应的经纬度
	float Long;//对应的经纬度
	float CoaseRate;//糙率
	float WaterHeight_New;//新的水高
	float WaterHeight_Old;//旧的水高
	float XUnitFlux_New; //x方向新的单宽流量(右邻格流到该格的单宽流量，负号表示是从该格往右邻格流)
	float YUnitFlux_New; //y方向新的单宽流量(上邻格流到该格的单宽流量，负号表示是从该格往上邻格流)
	float XUnitFlux_Old; //x方向旧的单宽流量
	float YUnitFlux_Old; //y方向旧的单宽流量
	float vec;	//绝对流速
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

class  CAFlood  
{
public:
	CAFlood();
	virtual ~CAFlood();	
//工程装载、保存、计算
	bool InitCellArray(char* strDataDir);
//计算下一个时间周期的元胞状态
	void Calcultate();

	//溃口相关计算
	void SetDamBreakInfo(float fDamVolume,float fDamWidth,float fDameHeight, float fBreakWidth,float fBreakHeight,bool bFullBreak,float fTypeFactor);
	void SetDamBreakInfo(DAMINFORMATION daminfo,float m_fTypeFactor,bool bFullBreak);
	void ComputeRushTimeANDQm();//计算泄空时间和峰值流量
	void ComputeTempFluxANDDepth(float t);//计算t时刻瞬时流量和瞬时水深
	double Integral(int y,double k[6]);//求五次函数的积分
	double sanci(double a,double b,long double c,long double d);//盛金公式求三次方程的根：
	
	//查询
	CACELL** GetCellArray();
	void ExportDepth(char* strDataPath);
	void ExportDepthJson(char* strDataPath);
	void ExportXVec(char* strDataPath);
	void ExportYVec(char* strDataPath);
	void ExportFileHeader(ofstream &fout);
private:
	void KillPots(long i,long j);
	void RepairVolume(long i,long j);
	void RepairFlux(long i,long j);
	void ComputeSourceFlood();
	void ComputeNewFloodFromHeight();
	void ComputeAverageWaterAndDiffHeight(int i, int j, float *fWaterDepth, float *fWaterDepthR, float *fWaterDepthU, 
		float *fAverWaterDepthX, float *fAverWaterDepthY, float *fDiffWaterHeightX, float *fDiffWaterHeightY);

	void ComputeXFlux(float fAverWaterDepthX, float fWaterDepthR, float fWaterDepth, 
		float fAverWaterDepthY,float fDiffWaterHeightX, int i, int j,float *xvec, float *yvec);

	void ComputeYFlux(float fAverWaterDepthY,float fWaterDepthU,float fWaterDepth,
		float fAverWaterDepthX,float fDiffWaterHeightY,int i, int j,float *xvec, float *yvec);
	void ComputeNewheightFromFlux(float *dH,float *fSumH);

public:
	float m_fCurTime;    //当前时间
	float m_fSpatialTimeRatio;
	float m_fRealDamVolume;
	float m_fSumQ;	
	double m_fRestVolume;
	long m_nCount;	
	DAMINFORMATION daminfo;
	DEMINFORMATION deminfo;

private:
	bool m_bFullBreak; //是否全溃
	float m_fTypeFactor; 	
	float m_fDamWaterHeight;	//t时刻水位高度
	float m_fDamWaterFlux;//t时刻水的流量	
	float m_fMaxFlux;		//最大流量
	float m_fBreakTime;  //泄空时间
	float m_fTimeUnit;

	double m_fCoefficiant[6];//拟合时间-流量用的多项式系数	
	long m_lRepairNum;//修复水量用	
	long m_nCellNum;
	RIVERINDEX *m_pOriginArray;//源头的位置数组
	CACELL** m_p2DCellArray;
	vector<RIVERINDEX> m_vector;
};
}
#endif // !defined(AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_)
