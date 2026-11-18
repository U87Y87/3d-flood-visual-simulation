#if !defined(AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_)
#define AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
#include<iostream>
#include <vector>
#include<sstream>
#include <fstream>//为引用系统头文件
#include "windows.h"
#include"CoordinateConversion.h"//为自定义头文件
using namespace std;


#define NODATA -9999
#define g 9.8           // syh:重力加速度
#define zero 0.00001   //syh: 极小值（用于浮点数判零）

//*元胞单元结构*//
typedef struct CACell
{
	bool b_IsRiver;//是否为河流区域  LL：用以标记该元胞是否有水
	bool b_IsOrigin;//是否为 源头——syh：溃口位置
	bool b_IsEnd;//是否为尾——syh：计算边界
	float DemData;//对应DEM高程/河床高度/DSM高程
	float Lat;//对应的纬度
	float Long;//对应的经度
	float CoaseRate;//糙率——syh：影响水流阻力计算（值越大阻力越大，流速越慢）
	float WaterHeight_New;//新的水深——syh：当前时间
	float WaterHeight_Old;//旧的水深——syh：上一时间
	float XUnitFlux_New; //x方向新的单宽流量(右邻格流到该格的单宽流量，负号表示是从该格往右邻格流)
	float YUnitFlux_New; //y方向新的单宽流量(上邻格流到该格的单宽流量，负号表示是从该格往上邻格流)
	float XUnitFlux_Old; //x方向旧的单宽流量——syh：上一时间
	float YUnitFlux_Old; //y方向旧的单宽流量——syh：上一时间
	float vec;	//绝对流速
}CACELL;

//*基流初始化用*//
//typedef struct RiverInput    //ylz 这个干嘛用？
//{
//	float RiverHight;
//	float XUnitFlux;
//	float YUnitFlux;
//	long x;
//	long y;
//}RIVERINPUT;

//*河道索引用*//
typedef struct RiverIndex    //ylz 用于对源头位置的定义，由一系列数组组成    后面有定义 RIVERINDEX *  m_poriginArray
{
	long x;
	long y;
}RIVERINDEX;

typedef struct DamInformation
{
	//大坝溃口信息
	float m_fDamVolume; //坝的库容
	float m_fDamWidth;  //坝宽
	float m_fDamHeight; //坝高
	float m_fBreakWidth; //溃口宽度
	float m_fBreakHeight;//溃口高度
}DAMINFORMATION;

typedef struct DEMInformation
{
	//高程信息
	int m_nWidth;//网格列数
	int m_nHeight;//网格行数
	float m_fCellsize;	//元胞尺寸
	float m_xllcorner;   //ylz  代表地图上面的实际坐标
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

	bool IsWaterSurfaceStable();
	//ghc水面平衡监测函数
	void Calcultate();

	//溃口相关计算
	void SetDamBreakInfo(float fDamVolume, float fDamWidth, float fDameHeight, float fBreakWidth, float fBreakHeight, bool bFullBreak, float fTypeFactor);
	void SetDamBreakInfo(DAMINFORMATION daminfo, float m_fTypeFactor, bool bFullBreak);
	void ComputeRushTimeANDQm();//计算泄空时间和峰值流量
	void ComputeTempFluxANDDepth(float t);//计算t时刻瞬时流量和瞬时水深
	double Integral(int y, double k[6]);//求五次函数的积分
	double sanci(double a, double b, long double c, long double d);//盛金公式求三次方程的根：


	//查询
	CACELL** GetCellArray();
	void ExportDepth(char* strDataPath);//输出深度
	void ExportDepthJson(char* strDataPath);//输出经纬度水深json
	void ExportXVec(char* strDataPath);//输出X方向流速
	void ExportYVec(char* strDataPath);//输出Y方向流速
	void ExportAVec(char* strDataPath);
	void ExportFileHeader(ofstream &fout);
private:
	void KillPots(long i, long j);
	void RepairVolume(long i, long j);
	void RepairFlux(long i, long j);

public:

	float m_fCurTime;                        // 当前时间
	float m_fSpatialTimeRatio;				//LL：时空比率，指元胞尺寸与计算单位时间的比值
	float m_fRealDamVolume;					//LL：真实溃出大坝的库容，指大坝的水从溃口流出的水量大小
	float m_fSumQ;							//LL：计算出的溃坝库容
	double m_fRestVolume;					//LL：剩余库容，指溃坝库容减去当前时刻流出去的水量大小
	long m_nCount;	    //ylz   总元胞数
	DAMINFORMATION daminfo;
	DEMINFORMATION deminfo;

private:
	bool m_bFullBreak;                   //   是否全溃
	float m_fTypeFactor; 	                                 // ylz   谢任之公式中的M	LL:表示断面为矩形
	float m_fDamWaterHeight;	  //   t时刻源头水位高度
	float m_fDamWaterFlux;       //    t时刻源头水的流量	
	float m_fMaxFlux;		           //    峰值流量
	float m_fBreakTime;          //     泄空时间
	float m_fTimeUnit;

	double m_fCoefficiant[6];            //     拟合时间-流量用的多项式系数	
	long m_lRepairNum;                   //       修复水量用	
	long m_nCellNum;                      //ylz   源头元胞总数（根据地形分辨率来的,这里是20m的，故为4个）
	RIVERINDEX *m_pOriginArray;//源头的位置数组 
	CACELL** m_p2DCellArray;//LL：所有元胞数组
	vector<RIVERINDEX> m_vector;//LL：有值元胞的集合，包括源头元胞


};
#endif // !defined(AFX_CAFLOOD_H__1A88276A_FC79_4ADA_9DAF_03444A88E43F__INCLUDED_)