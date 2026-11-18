// CAFloodNEW.cpp : 定义控制台应用程序的入口点。
//
#include <windows.h>
#include "stdafx.h"
#include "CAFloodNEW.h"
#include <omp.h>
#include <math.h>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace std;
static int InferUtmZoneFromCells(CACELL** cells, int height, int width)
{
	const double radToDeg = 180.0 / 3.14159265358979323846;
	for (int i = 0; i < height; ++i)
	{
		for (int j = 0; j < width; ++j)
		{
			double lon = cells[i][j].Long * radToDeg;
			if (lon >= -180.0 && lon < 180.0 && fabs(lon) > 0.000001)
			{
				int zone = static_cast<int>(floor((lon + 180.0) / 6.0)) + 1;
				if (zone < 1) zone = 1;
				if (zone > 60) zone = 60;
				return zone;
			}
		}
	}
	return 48;
}

static string FormatJsonNumber(double value)
{
	if (value != value || value == numeric_limits<double>::infinity() || value == -numeric_limits<double>::infinity())
	{
		return "0";
	}
	if (fabs(value - round(value)) < 0.0000000001)
	{
		ostringstream oss;
		oss << static_cast<long long>(round(value));
		return oss.str();
	}

	ostringstream oss;
	oss << fixed << setprecision(10) << value;
	string text = oss.str();
	while (!text.empty() && text.back() == '0')
	{
		text.pop_back();
	}
	if (!text.empty() && text.back() == '.')
	{
		text.pop_back();
	}
	return text.empty() ? "0" : text;
}


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
//#define new DEBUG_NEW
#endif

CAFlood::CAFlood()
{
	m_p2DCellArray = NULL;//元胞数组
	m_pOriginArray = NULL;//源头位置数组
	m_nCount = 0;
	m_fSpatialTimeRatio = 100;//设置时空比率
	m_fTimeUnit = 0;
	m_fSumQ = 0.0;
}

CAFlood::~CAFlood()
{
	if (m_p2DCellArray != NULL)                           //  ylz     对二维数组进行析构
	{
		for (int i = 0; i<deminfo.m_nHeight; i++)
		{
			delete[]m_p2DCellArray[i];
			m_p2DCellArray[i] = NULL;
		}
		delete[]m_p2DCellArray;
		m_p2DCellArray = NULL;
	}
	if (m_pOriginArray != NULL)
	{
		delete[]m_pOriginArray;
		m_pOriginArray = NULL;
	}
}

//void CAFlood::SetDamBreakInfo(float fDamVolume, float fDamWidth, float fDameHeight, float fBreakWidth, float fBreakHeight, bool bFullBreak, float fTypeFactor)
//{
//	daminfo.m_fDamVolume = fDamVolume;
//	daminfo.m_fDamWidth = fDamWidth;
//	daminfo.m_fDamHeight = fDameHeight;
//	daminfo.m_fBreakHeight = fBreakHeight;
//	daminfo.m_fBreakWidth = fBreakWidth;
//	this->m_fTypeFactor = m_fTypeFactor;
//	m_bFullBreak = bFullBreak;
//}                     // ylz             用于对结构体中的变量进行初始化

void CAFlood::SetDamBreakInfo(DAMINFORMATION daminfo, float m_fTypeFactor, bool bFullBreak)
{
	this->daminfo.m_fDamVolume = daminfo.m_fDamVolume;
	this->daminfo.m_fDamWidth = daminfo.m_fDamWidth;
	this->daminfo.m_fDamHeight = daminfo.m_fDamHeight;
	this->daminfo.m_fBreakHeight = daminfo.m_fBreakHeight;
	this->daminfo.m_fBreakWidth = daminfo.m_fBreakWidth;
	this->m_fTypeFactor = m_fTypeFactor;	//m：溃口形状系数，矩形断面m=1
	m_bFullBreak = bFullBreak;//全溃
}

void CAFlood::ComputeTempFluxANDDepth(float t)                     //  ylz    计算溃口的水高和流量    t为当前的时间
{
	//float FluxTemp=0;
	/*瞬时流量计算*/
	//因为拟合曲线时存在点位误差，头尾两点不能照顾全，做一个映射，让头尾两点到正确的位置
	double k0 = m_fCoefficiant[5];
	double k1 = m_fCoefficiant[0] * pow(m_fBreakTime, 5) + m_fCoefficiant[1] * pow(m_fBreakTime, 4) + m_fCoefficiant[2] * pow(m_fBreakTime, 3) + m_fCoefficiant[3] * pow(m_fBreakTime, 2) + m_fCoefficiant[4] * m_fBreakTime + m_fCoefficiant[5];
	double kx = m_fCoefficiant[0] * pow(t, 5) + m_fCoefficiant[1] * pow(t, 4) + m_fCoefficiant[2] * pow(t, 3) + m_fCoefficiant[3] * pow(t, 2) + m_fCoefficiant[4] * t + m_fCoefficiant[5];
	//ylz  通过参数的比值，得到一个最终的系数，其和最大流量的乘积即为当前的流量
	double kt = 1 + (kx - k0) / (k0 - k1);
	m_fDamWaterFlux = kt*m_fMaxFlux;   //ylz         当前的流量              
	//控制小数的位数
	//保留两位小数则乘100，n位乘10的n次方，加0.5为四舍五入
	m_fDamWaterFlux = (double)((int)(m_fDamWaterFlux * 100 + 0.5)) / 100.0;//t时刻的瞬时流量

	/*瞬时水位计算*/
	//库容水位曲线关系y = 70.243x3 - 784x2 + 8304.5x

	m_fRestVolume = m_fRealDamVolume - Integral((int)t, m_fCoefficiant)*m_fMaxFlux;//剩余库容   

	if (m_fRestVolume<0)
	{
		m_fRestVolume = 0;
	}
	double depth;//计算水深
	depth = sanci(70.243, -784, 8304.5, -m_fRestVolume);    //ylz   	看上面的注释   库容水位曲线关系y = 70.243x3 - 784x2 + 8304.5x    LL：y表示库容，x表示此库容对应的水位
	//ylz   剩余的水深
	double depth0 = sanci(70.243, -784, 8304.5, -m_fRealDamVolume);    	//ylz   总共的水深
	double depth1 = 0;
	m_fDamWaterHeight = 61.5*(depth - depth1) / (depth0 - depth1);//调整计算误差，使整个数据落在0-100米范围内
	// ylz   t时刻水位高度
	//ylz    t时刻水位高度不应该就是depth吗？？？为什么还要这样计算？？？
	if (m_fDamWaterHeight<0)
	{
		m_fDamWaterHeight = 0;
	}
	//控制小数的位数
	m_fDamWaterHeight = (double)((int)(m_fDamWaterHeight * 10 + 0.5)) / 10.0 + 725.8;//瞬时水位  
	return;
}

double CAFlood::Integral(int y, double k[6])                       // ylz  洪水文档中的1.2四次抛物线
{
	//求以k[6]数组六个元素为多项式系数的五次多项式的0-y范围内的积分
	double z;
	z = k[0] * pow((double)y, 6) / 6.0 + k[1] * pow((double)y, 5) / 5.0 + k[2] * pow((double)y, 4) / 4.0 + k[3] * pow((double)y, 3) / 3.0 + k[4] * pow((double)y, 2) / 2.0 + k[5] * y;
	return z;
}

double CAFlood::sanci(double a, double b, long double c, long double d)
{
	double  td = 0.333333;

	//盛金公式求三次方程的根：
	//本例根据曲线，适用于盛金公式中一实根情形
	double x;
	double A = pow(b, 2) - 3 * a*c;
	double B = b*c - 9 * a*d;
	double C = pow(c, 2) - 3 * b*d;
	double derta = pow(B, 2) - 4 * A*C;
	double y1, y2, x1, x2;
	y1 = A*b + 3 * a*(-B + pow((pow(B, 2) - 4 * A*C), 0.5)) / 2.0;
	y2 = A*b + 3 * a*(-B - pow((pow(B, 2) - 4 * A*C), 0.5)) / 2.0;
	if (y1<0)
	{
		x1 = -pow(-y1, td);
	}
	else
	{
		x1 = pow(y1, td);
	}
	if (y2<0)
	{
		x2 = -pow(-y2, td);
	}
	else
	{
		x2 = pow(y2, td);
	}
	x = (-b - x1 - x2) / (3 * a);

	return x;
}

//计算峰值流量和泄空时间
void CAFlood::ComputeRushTimeANDQm()
{
	//计算峰值流量Qm
	double lamda, Qm;
	float fRealDamVolume;//实际溃出库容
	//不同的溃决方式只是在计算最大库容和最大流量上有差别

	if (m_bFullBreak)//全溃情况
	{
		m_fRealDamVolume = daminfo.m_fDamVolume;//LL：真实溃出大坝的库容=大坝全部容量
		double temp = 2 * pow((double)m_fTypeFactor, 0.5) / (double)(1 + 2 * (double)m_fTypeFactor);    // ylz  谢任之计算公式      pow指数函数  ( u0=0,原因不清楚）
		lamda = pow((double)m_fTypeFactor, (double)m_fTypeFactor - 1)*pow((double)temp, (double)2 * (double)m_fTypeFactor + 1);//流量系数
		Qm = lamda*(double)daminfo.m_fDamWidth*pow(g, 0.5)*pow((double)daminfo.m_fDamHeight, 1.5);//坝址处最大洪峰流量
	}
	else//瞬时局部溃决情况
	{
		double h = daminfo.m_fDamHeight - daminfo.m_fBreakHeight;//坝高-溃口高
		if (m_fTypeFactor == 1)            //对矩形情况下进行简化分析，这样貌似准确点
		{
			m_fRealDamVolume = daminfo.m_fDamVolume* daminfo.m_fBreakHeight / daminfo.m_fDamHeight;//LL：真实溃出大坝的库容=溃口流出水量
		}
		else
		{
			m_fRealDamVolume = daminfo.m_fDamVolume - 70.243*pow(h, 3) + 784 * pow(h, 2) - 8304.5*h;//LL：真实溃出大坝的库容=大坝库容-剩余库容
		}
		//黄河水利委员会公式（垂向）
		Qm = (8.0 / 27.0)*pow((double)g, 0.5)*pow((double)daminfo.m_fDamWidth / (double)daminfo.m_fBreakWidth, 0.4)*pow((double)(11 * daminfo.m_fDamHeight - 10 * daminfo.m_fBreakHeight) / (double)daminfo.m_fDamHeight, 0.3)*daminfo.m_fBreakWidth*pow((double)daminfo.m_fBreakHeight, 1.5);
	}

	//控制小数的位数
	//保留两位小数则乘100，n位乘10的n次方，加0.5为四舍五入
	m_fMaxFlux = (double)((int)(Qm * 100 + 0.5)) / 100.0;//LL：峰值流量

	//计算泄空时间m_fBreakTime
	long tx1 = 0, tx2;
	double T0 = 4 * m_fRealDamVolume / (double)m_fMaxFlux;     //ylz  根据洪水文档1.2中第一步，其中k=4
	double dertaW = m_fRealDamVolume;//LL：真实溃出大坝的库容
	double test1;
	while (dertaW>0)                                                                      // ylz     计算六个参数     //ylz     拟合时间-流量用的多项式系数	
	{          //五次多项式拟合
		m_fCoefficiant[0] = -19.565 / (double)pow(T0, 5);
		m_fCoefficiant[1] = 56.595 / (double)pow(T0, 4);
		m_fCoefficiant[2] = -61.153 / (double)pow(T0, 3);
		m_fCoefficiant[3] = 30.574 / (double)pow(T0, 2);
		m_fCoefficiant[4] = -7.43 / (double)T0;
		m_fCoefficiant[5] = 0.9723;
		do
		{
			tx2 = tx1;
			tx1 = tx1 + 100;
		} while (Integral(tx2, m_fCoefficiant)<Integral(tx1, m_fCoefficiant));   //ylz    通过+100, +10, +1不断减少时间上的拟合之间值
		int tips100 = tx2;
		tx1 = tx2;
		do
		{
			tx2 = tx1;
			tx1 = tx1 + 10;
		} while (Integral(tx2, m_fCoefficiant)<Integral(tx1, m_fCoefficiant) && tx1<(tips100 + 100));//Integral 求五次函数的积分
		int tips10 = tx2;
		tx1 = tx2;
		do
		{
			tx2 = tx1;
			tx1 = tx1 + 1;
		} while (Integral(tx2, m_fCoefficiant)<Integral(tx1, m_fCoefficiant) && tx1<(tips10 + 10));
		T0 = double(tx2);
		test1 = Integral(tx2, m_fCoefficiant)*Qm;        //ylz   利用拟合好的曲线和最大流量相乘即理论上计算出的溃坝水量
		dertaW = m_fRealDamVolume - test1;              //ylz   计算出来的理论值和实际值相比较，然后再不断循环，直至dertaw小于0为止


	};//计算泄空时间，调整T0的值，使得到达泄空时间时库容为0
	m_fBreakTime = T0;//LL：泄空时间
	return;
}

bool CAFlood::InitCellArray(char* strDataDir)
{//初始化场景参数
	int nWidth;//dem范围大小（列数）
	int nHeight;//dem范围大小（行数）
	int i, j;
	float fCellsize;//元胞尺寸
	float* pDemdata = NULL;

	char strDemPath[80];//存储数据名称
	char strCoarseRate[80];
	char strRiverInit[80];
	strcpy(strDemPath, strDataDir);
	strcpy(strCoarseRate, strDataDir);
	strcpy(strRiverInit, strDataDir);
	


	// 统一使用 '/' 拼接路径V0711
	snprintf(strDemPath, sizeof(strDemPath), "%s/dem.asc", strDataDir);
	snprintf(strCoarseRate, sizeof(strCoarseRate), "%s/cao.asc", strDataDir);
	snprintf(strRiverInit, sizeof(strRiverInit), "%s/InputRiver.dat", strDataDir);

	/*读进DEM数据,分配cell数组*/
	FILE * file = fopen(strDemPath, "r");
	if (file == NULL)
	{
		//AfxMessageBox("打开DEM文件失败!");
		cerr << "打开DEM文件失败!";
		return false;
	}
	long nCount;
	fseek(file, 0L, SEEK_END);
	nCount = ftell(file);
	fseek(file, 0L, SEEK_SET);
	char *pData = new char[nCount];
	fread(pData, 1, nCount, file);
	//读文件头
	strtok(pData, " \n");
	nWidth = atoi(strtok(NULL, " \n"));
	strtok(NULL, " \n");
	nHeight = atoi(strtok(NULL, " \n"));
	strtok(NULL, " \n");
	deminfo.m_xllcorner = atof(strtok(NULL, " \n"));
	strtok(NULL, " \n");
	deminfo.m_yllcorner = atof(strtok(NULL, " \n"));
	strtok(NULL, " \n");
	fCellsize = atof(strtok(NULL, " \n"));
	strtok(NULL, "\n");
	deminfo.m_nWidth = nWidth;
	deminfo.m_nHeight = nHeight;
	deminfo.m_fCellsize = fCellsize;
	m_fTimeUnit = deminfo.m_fCellsize / m_fSpatialTimeRatio;               // ylz  每计算一次所花的时间
	//分配CELL数组内存，初始化索引数组和部分CELL数组
	float temp = 0;
	RIVERINDEX index;
	m_p2DCellArray = new CACELL*[nHeight];                                       // ylz    二维数组的创建  创建元胞数组
	for (i = 0; i<nHeight; i++)
	{
		m_p2DCellArray[i] = new CACELL[nWidth];
	}
	float ll_i = 0;
	for (i = 0; i<nHeight; i++)
	for (j = 0; j<nWidth; j++)
	{
		cout << fixed << setprecision(2) << ll_i / (nHeight * nWidth) * 100 << " %" << endl;//打印进度 cout << ll_i / nHeight / nWidth << endl;//打印进度
		ll_i += 1;

		temp = atof(strtok(NULL, " \n"));      // ylz   获取一位置上的值,atof函数是将字符串转化为浮点数，strtok函数是将字符串分解为一组字符串（NULL是待分解字符串，"\n"是分隔）
		if ((i == 25))                                                   //ylz    调试用，不起作用
		{
			int ttt = 0;
		}
		if (temp != 9999)
		{
			index.x = j;//元胞列号
			index.y = i;//元胞行号
			m_vector.push_back(index);
		}

		if (temp != 9999){
			double X = this->deminfo.m_xllcorner + j*this->deminfo.m_fCellsize;
			double Y = this->deminfo.m_yllcorner + (nHeight - i)*this->deminfo.m_fCellsize;
			UTM2LatLon* c = new UTM2LatLon();
			double  LatLong[2];

			stringstream ss;                       //ylz 将其转化为C++的string

			stringstream strX, strY;
			strX << X;
			strY << Y;

			std::string s1 = strX.str();
			std::string s2 = strY.str();

			std::string ssss = "48 N " + s1 + " " + s2;

			c->convertUTMToLatLong(ssss, LatLong);                  //ylz  转化为经纬度

			m_p2DCellArray[i][j].Lat = LatLong[0] * 3.1415926535897932384626 / 180; //转换为弧度
			m_p2DCellArray[i][j].Long = LatLong[1] * 3.1415926535897932384626 / 180;//转换为弧度
		}
		else
		{
			m_p2DCellArray[i][j].Lat = 0;                //转换为弧度
			m_p2DCellArray[i][j].Long = 0;           //转换为弧度
		}

		m_p2DCellArray[i][j].DemData = temp;
		m_p2DCellArray[i][j].WaterHeight_New = 0;
		m_p2DCellArray[i][j].WaterHeight_Old = temp;
		m_p2DCellArray[i][j].XUnitFlux_New = 0;
		m_p2DCellArray[i][j].XUnitFlux_Old = 0;
		m_p2DCellArray[i][j].YUnitFlux_New = 0;
		m_p2DCellArray[i][j].YUnitFlux_Old = 0;
		m_p2DCellArray[i][j].b_IsRiver = false;
		m_p2DCellArray[i][j].b_IsOrigin = false;
		m_p2DCellArray[i][j].b_IsEnd = false;
		m_p2DCellArray[i][j].vec = 0;
	}
	//销毁临时内存，关闭文件
	delete[]pData;
	fclose(file);

	/*读入糙率和基流数据,初始化部分cell数组*/
	//读入糙率数据,直接放在元胞数组里
	file = fopen(strCoarseRate, "r");
	if (file == NULL) {
		cerr << "[ERROR] 打开糙率文件失败: " << strCoarseRate << endl;
		return false;
	}
	fseek(file, 0L, SEEK_END);
	nCount = ftell(file);
	fseek(file, 0L, SEEK_SET);

	pData = new char[nCount];
	fread(pData, 1, nCount, file);
	strtok(pData, "\n");
	strtok(NULL, "\n");
	strtok(NULL, "\n");
	strtok(NULL, "\n");
	strtok(NULL, "\n");
	strtok(NULL, "\n");

	for (i = 0; i<nHeight; i++)
	for (j = 0; j<nWidth; j++)
	{
		if ((i == 25) && (j == 10))
		{
			int ttt = 0;
		}
		m_p2DCellArray[i][j].CoaseRate = atof(strtok(NULL, " \n"));
	}
	delete[]pData;
	fclose(file);
	//读入基流数据,存入基流数组pRiArray中
	file = fopen(strRiverInit, "r");
	fseek(file, 0L, SEEK_END);
	nCount = ftell(file);
	fseek(file, 0L, SEEK_SET);
	pData = new char[nCount];
	fread(pData, 1, nCount, file);
	m_nCellNum = atoi(strtok(pData, " \n"));//atoi函数（将字符串转换成整型数）  LL：源头元胞总数
	m_pOriginArray = new RIVERINDEX[m_nCellNum];
	for (i = 0; i<m_nCellNum; i++)
	{
		m_pOriginArray[i].x = atof(strtok(NULL, " \n"));
		m_pOriginArray[i].y = atof(strtok(NULL, " \n"));
	}
	delete[]pData;
	fclose(file);
	int m, n = 0;
	for (i = 0; i<m_nCellNum; i++)
	{
		n = m_pOriginArray[i].x;//源头位置列号
		m = m_pOriginArray[i].y;//源头位置行号
		m_p2DCellArray[nHeight - m][n].b_IsRiver = true;
		m_p2DCellArray[nHeight - m][n].b_IsOrigin = true;
		m_p2DCellArray[nHeight - m][n].DemData = 9999;
		m_p2DCellArray[deminfo.m_nHeight - m][n].WaterHeight_New = 9999;
		m_p2DCellArray[deminfo.m_nHeight - m][n].WaterHeight_Old = 9999;
	}
	//对上下边界进行初始化
	for (i = 0; i<deminfo.m_nWidth; i++)
	{
		m_p2DCellArray[0][i].DemData = 9999;
		m_p2DCellArray[0][i].WaterHeight_New = 9999;
		m_p2DCellArray[0][i].WaterHeight_Old = 9999;
		m_p2DCellArray[deminfo.m_nHeight - 1][i].DemData = 9999;
		m_p2DCellArray[deminfo.m_nHeight - 1][i].WaterHeight_New = 9999;
		m_p2DCellArray[deminfo.m_nHeight - 1][i].WaterHeight_Old = 9999;
	}
	//对左右边界进行初始化
	for (i = 0; i<deminfo.m_nHeight; i++)
	{
		m_p2DCellArray[i][0].DemData = 9999;
		m_p2DCellArray[i][0].WaterHeight_New = 9999;
		m_p2DCellArray[i][0].WaterHeight_Old = 9999;
		m_p2DCellArray[i][deminfo.m_nWidth - 1].DemData = 9999;
		m_p2DCellArray[i][deminfo.m_nWidth - 1].WaterHeight_New = 9999;
		m_p2DCellArray[i][deminfo.m_nWidth - 1].WaterHeight_Old = 9999;
	}

	m_fCurTime = 0;    //当前时间归0

	return true;
}

//更新元胞中的水深
void CAFlood::Calcultate()
{
	//重新计算溃口的水高和流量
	if (m_fCurTime <= m_fBreakTime)
	{
		ComputeTempFluxANDDepth(m_fCurTime);//LL：计算t时刻溃口处的瞬时流量和瞬时水位   只需要用到瞬时流量
	}
	//初始化CELL数组时要经过点处理，m_fDamWaterFlux为t时刻1S内所有格网的流量总和
	//当前所获得的是2*m_fTimeUnit流经每个格子每个方向上的流量
	int m, n = 0;
	int i, j;
	/*计算源头元胞的单宽流量*/
	//LL：此循环循环数量太少，不必并行计算
	for (i = 0; i<m_nCellNum; i++)
	{
		//ylz   m_fTimeUnit   
		//ylz   利用时空比计时    时空比=空间单位/时间单位,设时空比为100，这里空间单位为20,从而  m_fTimeUnit=0.2  
		//LL 源头位置数组
		n = m_pOriginArray[i].x;//列号
		m = m_pOriginArray[i].y;//行号
		m_p2DCellArray[deminfo.m_nHeight - m][n].XUnitFlux_Old = (m_fDamWaterFlux * 2 * m_fTimeUnit) / (2 * m_nCellNum*deminfo.m_fCellsize*m_fTimeUnit);
		//ylz   计算单宽流量：    容量除以元胞个数即单个元胞的容量
		// ylz                        再除以元胞大小即单个元胞单位内的容量 
		// ylz                        最后除以时间即每秒范围内单个元胞单位内的容量 
		//ylz  最终溃口元胞的水量都是一样的
		m_p2DCellArray[deminfo.m_nHeight - m][n].XUnitFlux_New = m_p2DCellArray[deminfo.m_nHeight - m][n].XUnitFlux_Old;
		m_p2DCellArray[deminfo.m_nHeight - m][n].YUnitFlux_Old = m_p2DCellArray[deminfo.m_nHeight - m][n].XUnitFlux_Old;
		m_p2DCellArray[deminfo.m_nHeight - m][n].YUnitFlux_New = m_p2DCellArray[deminfo.m_nHeight - m][n].XUnitFlux_Old;
	}

	float fWaterDepth,//当前格网水深
		fWaterDepthR, //右边格网水深
		fWaterDepthU;//下边格网水深
	float fAverWaterDepthX, //当前格网和右边格网的平均水深
		fAverWaterDepthY;//当前格网和下边格网的平均水深
	float fAverWaterHeightX;
	float fAverWaterHeightY;
	float fDiffWaterHeightX;//当前格水位比右边高出的部分
	float fDiffWaterHeightY;//当前格水位比下面高出的部分
	float xvec = 0;
	float yvec = 0;
	m_nCount++;
	float temp1, temp2, temp3; //单宽流量变化率计算公式中的临时量
	int IndexCount;//循环中的顺序标志
	float dH;//LL：水位变化，从该元胞左侧和上侧两个元胞流入的水深
	float fSumH = 0.0;//LL：所有非源头元胞水深值相加

	omp_set_num_threads(16);//设置x个线程
	//	/*计算非源头元胞的单宽流量*/
#pragma omp parallel default(none) private(IndexCount,i,j,fWaterDepth,fWaterDepthR,fWaterDepthU,fAverWaterDepthX,fAverWaterDepthY,fDiffWaterHeightX, fDiffWaterHeightY,xvec,temp2,yvec,temp1,temp3,dH) reduction(+:fSumH)//使用静态调度
	{
#pragma omp for schedule(static)
	for (IndexCount = 0; IndexCount<m_vector.size(); IndexCount++)
	{
		i = m_vector[IndexCount].y;//纵坐标，多少行
		j = m_vector[IndexCount].x;//横坐标，多少列
		if ((i == 0) || (j == 0) || (i == deminfo.m_nHeight - 1) || (j == deminfo.m_nWidth - 1))
		{//如果元胞处于四个边界处，无法计算，需要跳过
			continue;
		}
		//非源头元胞数据计算开始
		//old表示t时刻，new表示t+1时刻
		
		if (m_p2DCellArray[i][j].b_IsOrigin == false) 
		{//判断是否是非源头元胞
			fWaterDepth = m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData;//计算当前格网水深
			fWaterDepthR = m_p2DCellArray[i][j + 1].WaterHeight_Old - m_p2DCellArray[i][j + 1].DemData;//计算右边格网水深
			fWaterDepthU = m_p2DCellArray[i + 1][j].WaterHeight_Old - m_p2DCellArray[i + 1][j].DemData;//计算下边格网水深
			fAverWaterDepthX = 0.5*(fWaterDepth + fWaterDepthR);//当前格网和右边格网的平均水深
			fAverWaterDepthY = 0.5*(fWaterDepth + fWaterDepthU);//当前格网和下边格网的平均水深
			fDiffWaterHeightX = m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j + 1].WaterHeight_Old;//当前格水位比右边高出的部分
			fDiffWaterHeightY = m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i + 1][j].WaterHeight_Old;//当前格水位比下面高出的部分
			
			/*x方向新单宽流量*/
			//如果一个格子的水面高度低于另一个格子的地面，且另一个格子没有水，或者两个格子都没有水，则不发生流动
			if ((fAverWaterDepthX <= zero) ||
				((m_p2DCellArray[i][j].WaterHeight_Old<m_p2DCellArray[i][j + 1].DemData) && fWaterDepthR <= zero) ||
				((m_p2DCellArray[i][j + 1].WaterHeight_Old<m_p2DCellArray[i][j].DemData) && fWaterDepth <= zero))
			{
				m_p2DCellArray[i][j].XUnitFlux_New = 0;
			}
			else//发生流动的情况
			{
				//ylz   xvec,yvec为流速

				//LL：根据旧的单宽流量算速度(单位:米/每秒) 
				xvec = m_p2DCellArray[i][j].XUnitFlux_Old / fAverWaterDepthX;
				if (fabs(xvec)<zero)     //ylz  判断没有意义
					temp2 = 0;
				else
				{
					if (fAverWaterDepthY <= zero)
						yvec = 0;
					else
						//由于单宽流量M=u*h，N=v*h
						yvec = m_p2DCellArray[i][j].YUnitFlux_Old / fAverWaterDepthY;
					temp1 = pow((float)fAverWaterDepthX, (float)(1.0 / 3.0));	//LL:计算单宽流量公式中第三部分的分母
					//	temp2= 2*g*dt*m_p2DCellArray[i][j].CoaseRate*xvec*sqrt(xvec*xvec+yvec*yvec)/temp1;
					temp2 = g*m_fTimeUnit*m_p2DCellArray[i][j].CoaseRate*m_p2DCellArray[i][j].CoaseRate*
						xvec*sqrt(xvec*xvec + yvec*yvec) / temp1;              // ylz      计算x方向的单宽流量式中的第三部分
					if (fabs(m_p2DCellArray[i][j].XUnitFlux_Old)<fabs(temp2))     //ylz   这个判断是什么意思？  用于处理异常情况    m_p2DCellArray[i][j].XUnitFlux_Old即为t时刻的单宽流量
						temp2 = m_p2DCellArray[i][j].XUnitFlux_Old;
				}
				temp3 = g*m_fTimeUnit*fAverWaterDepthX*fDiffWaterHeightX / deminfo.m_fCellsize;//LL:计算单宽流量公式中的第二部分
				m_p2DCellArray[i][j].XUnitFlux_New = m_p2DCellArray[i][j].XUnitFlux_Old - (temp2 - temp3);    // ylz  x方向的（首先由t时刻的水深计算t+1时刻的）单宽流量
			}

			//y方向新单宽流量
			if ((fAverWaterDepthY <= zero) ||
				((m_p2DCellArray[i][j].WaterHeight_Old<m_p2DCellArray[i + 1][j].DemData) && fWaterDepthU <= zero) ||
				((m_p2DCellArray[i + 1][j].WaterHeight_Old<m_p2DCellArray[i][j].DemData) && fWaterDepth <= zero))
			{
				m_p2DCellArray[i][j].YUnitFlux_New = 0;
			}
			else
			{
				yvec = m_p2DCellArray[i][j].YUnitFlux_Old / fAverWaterDepthY;
				if (fabs(yvec)<zero)
					temp2 = 0;
				else
				{
					if (fAverWaterDepthX <= zero)
						xvec = 0;
					else
						xvec = m_p2DCellArray[i][j].XUnitFlux_Old / fAverWaterDepthX;
					temp1 = pow((float)fAverWaterDepthY, (float)(1.0 / 3.0));
					//temp2= 2*g*dt*m_p2DCellArray[i][j].CoaseRate*yvec*sqrt(xvec*xvec+yvec*yvec)/temp1;
					temp2 = g*m_fTimeUnit*m_p2DCellArray[i][j].CoaseRate*m_p2DCellArray[i][j].CoaseRate*
						yvec*sqrt(xvec*xvec + yvec*yvec) / temp1;
					if (fabs(m_p2DCellArray[i][j].YUnitFlux_Old)<fabs(temp2))
						temp2 = m_p2DCellArray[i][j].YUnitFlux_Old;
				}
				temp3 = g*m_fTimeUnit*fAverWaterDepthY*fDiffWaterHeightY / deminfo.m_fCellsize;
				m_p2DCellArray[i][j].YUnitFlux_New = m_p2DCellArray[i][j].YUnitFlux_Old - (temp2 - temp3);//y方向新的单宽流量
			}
			RepairFlux(i, j);
			m_p2DCellArray[i][j].vec = sqrt(xvec*xvec + yvec*yvec);//速度
			m_p2DCellArray[i][j].XUnitFlux_Old = m_p2DCellArray[i][j].XUnitFlux_New;//新的x方向的单款流量赋值到旧单宽流量中
			m_p2DCellArray[i][j].YUnitFlux_Old = m_p2DCellArray[i][j].YUnitFlux_New;//新的y方向的单款流量赋值到旧单宽流量中
		}
	}
	
	/*计算新的水面高度*/
#pragma omp for schedule(static)
	for (IndexCount = 0; IndexCount<m_vector.size(); IndexCount++)
	{
		i = m_vector[IndexCount].y;//行数
		j = m_vector[IndexCount].x;//列数
		//这里需要对源头的背面进行严格的界定
		// ylz    跟模型没有关系
		if ((i == 27) && (j == 15) && (m_fCurTime>6.7))
		{
			int ttt = 0;
		}

		//ylz 运行到边界时	LL：边界处元胞不更新状态
		if ((i == 0) || (j == 0) || (i == deminfo.m_nHeight - 1) || (j == deminfo.m_nWidth - 1))
		{
			continue;
		}
		//维护非源头的新水位
		if (m_p2DCellArray[i][j].b_IsOrigin == false)
		{
			dH = (m_p2DCellArray[i][j - 1].XUnitFlux_New - m_p2DCellArray[i][j].XUnitFlux_New +
				m_p2DCellArray[i - 1][j].YUnitFlux_New - m_p2DCellArray[i][j].YUnitFlux_New)*m_fTimeUnit / deminfo.m_fCellsize;
			m_p2DCellArray[i][j].WaterHeight_New = m_p2DCellArray[i][j].WaterHeight_Old + dH;//计算得到的新水位
			//ylz     再由t+1时刻的单宽流量计算t+2时刻的水深
			//ylz     dH其计算公式中的后两部分

			if (m_p2DCellArray[i][j].WaterHeight_New <= m_p2DCellArray[i][j].DemData)
			{//地形高大于或等于水位高
				m_p2DCellArray[i][j].WaterHeight_New = m_p2DCellArray[i][j].DemData;
				m_p2DCellArray[i][j].b_IsRiver = false;
			}
			else
				m_p2DCellArray[i][j].b_IsRiver = true;
			//特殊处理，用于处理水洼			
			KillPots(i, j);
			if ((m_fSumQ > m_fRealDamVolume + 6000) && (m_lRepairNum > 0))
			{
				RepairVolume(i, j);
			}
			//特殊处理
			m_p2DCellArray[i][j].WaterHeight_Old = m_p2DCellArray[i][j].WaterHeight_New;//将计算的新水位赋值到旧水位中
			m_p2DCellArray[i][j].XUnitFlux_Old = m_p2DCellArray[i][j].XUnitFlux_New;
			m_p2DCellArray[i][j].YUnitFlux_Old = m_p2DCellArray[i][j].YUnitFlux_New;
		}
		fSumH += m_p2DCellArray[i][j].WaterHeight_New - m_p2DCellArray[i][j].DemData;//所有非源头元胞水深值总和
		//统计最值
		//if (m_p2DCellArray[i][j].b_IsRiver)
		//{
		//	if(m_maxvec<m_p2DCellArray[i][j].vec)
		//		m_maxvec= m_p2DCellArray[i][j].vec;
		//	if (m_minvec>m_p2DCellArray[i][j].vec) 
		//		m_minvec= m_p2DCellArray[i][j].vec;
		//}
		//if (m_maxdepth<(m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i][j].DemData))
		//	m_maxdepth=m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i][j].DemData;
		//if (m_mindepth>(m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i][j].DemData))
		//	m_mindepth=m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i][j].DemData;

	}
#pragma omp single//两个循环执行后，由一个线程执行该部分语句，且其余线程必须等待该线程执行完毕才能开始其余操作
	{
	m_fCurTime += 2 * m_fTimeUnit;//当前时间
	m_fSumQ = fSumH* deminfo.m_fCellsize* deminfo.m_fCellsize + m_fRestVolume;  //ylz    m_fSumQ与 fRealDamVolume理论上应该是相等的		LL：溃坝库容=溃出水库的水量+剩余库容

	//stringstream str1;
	//str1<<m_fSumQ;
	//string str2=str1.str();
	//cout<<"总水量为"<<str2<<endl;
	//CString str;
	//str.Format("总水量为%f\n",m_fSumQ);
	//TRACE(str);

	m_lRepairNum = 12000 / (deminfo.m_fCellsize* deminfo.m_fCellsize);
	}
	}
}



void CAFlood::RepairVolume(long i, long j)         //ylz  为什么是>5，然后减去-0.5呢
{
	if ((m_p2DCellArray[i][j].WaterHeight_New - m_p2DCellArray[i][j].DemData)>5)
	{
		m_p2DCellArray[i][j].WaterHeight_New = m_p2DCellArray[i][j].WaterHeight_New - 0.5;
		m_lRepairNum--;
	}
}

//修正过大的流量
void CAFlood::RepairFlux(long i, long j)
{
	//这里要加上一个判断,修正过大的流量
	if ((m_p2DCellArray[i][j].YUnitFlux_New + m_p2DCellArray[i][j].XUnitFlux_New)*m_fTimeUnit>
		(m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize)
	{
		float Q = (m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize / m_fTimeUnit;
		float sum = (m_p2DCellArray[i][j].YUnitFlux_New + m_p2DCellArray[i][j].XUnitFlux_New);
		m_p2DCellArray[i][j].YUnitFlux_New = Q* m_p2DCellArray[i][j].YUnitFlux_New / sum;
		m_p2DCellArray[i][j].XUnitFlux_New = Q* m_p2DCellArray[i][j].XUnitFlux_New / sum;
	}
	//		//反方向修正
	if (-(m_p2DCellArray[i - 1][j].YUnitFlux_New + m_p2DCellArray[i][j - 1].XUnitFlux_New)*m_fTimeUnit>
		(m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize)
	{
		float Q = (m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize / m_fTimeUnit;
		float sum = (m_p2DCellArray[i][j - 1].XUnitFlux_New + m_p2DCellArray[i - 1][j].YUnitFlux_New);
		m_p2DCellArray[i - 1][j].YUnitFlux_New = Q* m_p2DCellArray[i - 1][j].YUnitFlux_New / sum;
		m_p2DCellArray[i][j - 1].XUnitFlux_New = Q* m_p2DCellArray[i][j - 1].XUnitFlux_New / sum;

	}
	//	float LeftDepth=m_p2DCellArray[i][j-1].WaterHeight_New-m_p2DCellArray[i][j-1].DemData;
	//	float RightDepth=m_p2DCellArray[i][j+1].WaterHeight_New-m_p2DCellArray[i][j+1].DemData;
	//	float UpperDepth=m_p2DCellArray[i+1][j].WaterHeight_New-m_p2DCellArray[i+1][j].DemData;
	//	float LowerDepth=m_p2DCellArray[i-1][j].WaterHeight_New-m_p2DCellArray[i-1][j].DemData;
	//	float Depth= m_p2DCellArray[i][j].WaterHeight_New-m_p2DCellArray[i][j].DemData;
	//	if ((LeftDepth+RightDepth+UpperDepth+LowerDepth+Depth)<0.5)
	//	{
	//		m_p2DCellArray[i][j].XUnitFlux_New=0;
	//		m_p2DCellArray[i][j].YUnitFlux_New=0;
	//
	//	}
}
void CAFlood::KillPots(long i, long j)                //ylz   用于处理水洼
{
	if ((i == 35) && (j == 11) && (m_fCurTime>27))
	{
		int ttt = 0;
	}
	float LeftDepth = m_p2DCellArray[i][j - 1].WaterHeight_New - m_p2DCellArray[i][j - 1].DemData;
	float RightDepth = m_p2DCellArray[i][j + 1].WaterHeight_New - m_p2DCellArray[i][j + 1].DemData;
	float UpperDepth = m_p2DCellArray[i + 1][j].WaterHeight_New - m_p2DCellArray[i + 1][j].DemData;
	float LowerDepth = m_p2DCellArray[i - 1][j].WaterHeight_New - m_p2DCellArray[i - 1][j].DemData;
	float LeftLowerDepth = m_p2DCellArray[i - 1][j - 1].WaterHeight_New - m_p2DCellArray[i - 1][j - 1].DemData;
	float LeftUpperDepth = m_p2DCellArray[i + 1][j - 1].WaterHeight_New - m_p2DCellArray[i + 1][j - 1].DemData;
	float RightLowerDepth = m_p2DCellArray[i - 1][j + 1].WaterHeight_New - m_p2DCellArray[i - 1][j + 1].DemData;
	float RightUpperDepth = m_p2DCellArray[i + 1][j + 1].WaterHeight_New - m_p2DCellArray[i + 1][j + 1].DemData;
	float Depth = m_p2DCellArray[i][j].WaterHeight_New - m_p2DCellArray[i][j].DemData;
	int nCount = 0;
	if (LeftDepth>0)
		nCount++;
	if (RightDepth>0)
		nCount++;
	if (UpperDepth>0)
		nCount++;
	if (LowerDepth>0)
		nCount++;
	if (LeftLowerDepth>0)
		nCount++;
	if (LeftUpperDepth>0)
		nCount++;
	if (RightLowerDepth>0)
		nCount++;
	if (RightUpperDepth>0)
		nCount++;

	if (m_fCurTime>10)   //ylz   时间大于10就开始进行判断
	{
		if ((nCount <= 2) && (Depth>0) && ((LeftDepth + RightDepth + UpperDepth + LowerDepth + LeftLowerDepth + LeftUpperDepth + RightLowerDepth + RightUpperDepth)<1))
		{
			m_p2DCellArray[i][j].WaterHeight_New = m_p2DCellArray[i][j].DemData;
			m_p2DCellArray[i][j].b_IsRiver = false;
		}
	}//ylz   对一个元胞的八邻域所有的水深进行计算，如果少于等于两个邻域没有水以及八邻域的所有水深值小于1则认为没有水
	//ylz   如果大于7个有水，且元胞没有水，则认为此元胞有水
	if ((nCount >= 7) && (Depth == 0))
	{
		m_p2DCellArray[i][j].WaterHeight_New = m_p2DCellArray[i][j].DemData + 0.1;
		m_p2DCellArray[i][j].b_IsRiver = true;
	}
}


void CAFlood::ExportDepthJson(char* strDataPath)
{
	if (m_p2DCellArray == NULL)
	{
		return;
	}

	string outPath(strDataPath);
	size_t dotPos = outPath.find_last_of('.');
	if (dotPos == string::npos)
	{
		outPath += ".json";
	}
	else
	{
		outPath = outPath.substr(0, dotPos) + ".json";
	}

	ofstream fout(outPath.c_str(), ios::out);
	if (!fout)
	{
		cout << "json create failed: " << outPath << endl;
		return;
	}

	int zone = InferUtmZoneFromCells(m_p2DCellArray, deminfo.m_nHeight, deminfo.m_nWidth);
	const string latZone = "N";
	UTM2LatLon converter;
	double maxDepth = 0.0;
	for (int i = 0; i < deminfo.m_nHeight; ++i)
	{
		for (int j = 0; j < deminfo.m_nWidth; ++j)
		{
			double depth = m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData;
			depth = round(depth * 100.0) / 100.0;
			if (depth > 0.0 && depth != 9999.0 && depth > maxDepth)
			{
				maxDepth = depth;
			}
		}
	}

	fout << "{min: 0,max: " << FormatJsonNumber(maxDepth) << ",points: [";

	for (int i = 0; i < deminfo.m_nHeight; ++i)
	{
		for (int j = 0; j < deminfo.m_nWidth; ++j)
		{
			double depth = m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData;
			depth = round(depth * 100.0) / 100.0;
			if (depth <= 0.0 || depth == 9999.0)
			{
				continue;
			}

			if (depth > maxDepth)
			{
				maxDepth = depth;
			}

			double x = deminfo.m_xllcorner + (j + 0.5) * deminfo.m_fCellsize;
			double y = deminfo.m_yllcorner + (deminfo.m_nHeight - i - 0.5) * deminfo.m_fCellsize;

			stringstream utm;
			utm << zone << " " << latZone << " " << fixed << setprecision(6) << x << " " << y;

			double latlon[2];
			converter.convertUTMToLatLong(utm.str(), latlon);
			double lat = round(latlon[0] * 10000000000.0) / 10000000000.0;
			double lon = round(latlon[1] * 10000000000.0) / 10000000000.0;

			fout << "{x:" << FormatJsonNumber(lon)
				<< ",y:" << FormatJsonNumber(lat)
				<< ",value:" << FormatJsonNumber(depth) << "},";
		}
	}

	fout << "]}";
	fout.close();
	cout << "json保存完毕: " << outPath << " (UTM zone " << zone << ")" << endl;

}
void CAFlood::ExportDepth(char* strDataPath)
{
	int i, j;
	//if (_access( strDataPath,0 ) != -1) 
	//{
	//	DeleteFile(strDataPath) ;
	//}
	ofstream fout(strDataPath, ios::out);

	//文件头的写入
	//fout<<"ncols         "<<deminfo.m_nWidth<<endl;
	//fout<<"nrows         "<<deminfo.m_nHeight<<endl;
	//fout<<"xllcorner     "<<deminfo.m_xllcorner<<endl;
	//fout<<"yllcorner     "<<deminfo.m_yllcorner<<endl;
	//fout<<"cellsize           "<<deminfo.m_fCellsize<<endl;
	//fout<<"NODATA_value  -9999"<<endl;
	ExportFileHeader(fout);	//LL：写入头文件
	fout << fixed << setprecision(2);

	//写出CELL数组	cell数组中的信息
	for (i = 0; i<deminfo.m_nHeight; i++)
	{
		for (j = 0; j<deminfo.m_nWidth - 1; j++)
		{
			float val = m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData;
			if (val == 0.0f) fout << 0 << " ";
			else if (val == 9999.0f) fout << 9999 << " ";
			else fout << val << " ";
		}
		float val = m_p2DCellArray[i][j].WaterHeight_Old - m_p2DCellArray[i][j].DemData;
		if (val == 0.0f) fout << 0 << endl;
		else if (val == 9999.0f) fout << 9999 << endl;
		else fout << val << endl;
	}

	fout.close();
	cout << "保存完毕!!" << endl;

}

void CAFlood::ExportXVec(char* strDataPath)
{
	int i, j;
	//if (_access( strDataPath,0 ) != -1) 
	//{
	//	DeleteFile(strDataPath) ;
	//}
	ofstream fout(strDataPath);
	//文件头的写入
	//fout<<"ncols         "<<deminfo.m_nWidth<<endl;
	//fout<<"nrows         "<<deminfo.m_nHeight<<endl;
	//fout<<"xllcorner     "<<deminfo.m_xllcorner<<endl;
	//fout<<"yllcorner     "<<deminfo.m_yllcorner<<endl;
	//fout<<"cellsize           "<<deminfo.m_fCellsize<<endl;
	//fout<<"NODATA_value  -9999"<<endl;
	ExportFileHeader(fout);
	fout << fixed << setprecision(2);
	float xvec = 0;
	//写出CELL数组
	for (i = 0; i<deminfo.m_nHeight; i++)
	{
		for (j = 0; j<deminfo.m_nWidth - 1; j++)
		{
			xvec = m_p2DCellArray[i][j].XUnitFlux_Old / m_p2DCellArray[i][j].WaterHeight_Old;
			if (xvec == 0.0f) fout << 0 << " ";
			else fout << xvec << " ";
		}
		xvec = m_p2DCellArray[i][j].XUnitFlux_Old / m_p2DCellArray[i][j].WaterHeight_Old;
		if (xvec == 0.0f) fout << 0 << endl;
		else fout << xvec << endl;
	}

	fout.close();
	cout << "保存完毕!!" << endl;
}

void CAFlood::ExportYVec(char* strDataPath)
{
	int i, j;
	//if (_access( strDataPath,0 ) != -1) 
	//{
	//	DeleteFile(strDataPath) ;
	//}
	ofstream fout(strDataPath);
	//文件头的写入
	//fout<<"ncols         "<<deminfo.m_nWidth<<endl;
	//fout<<"nrows         "<<deminfo.m_nHeight<<endl;
	//fout<<"xllcorner     "<<deminfo.m_xllcorner<<endl;
	//fout<<"yllcorner     "<<deminfo.m_yllcorner<<endl;
	//fout<<"cellsize           "<<deminfo.m_fCellsize<<endl;
	//fout<<"NODATA_value  -9999"<<endl;

	ExportFileHeader(fout);
	fout << fixed << setprecision(2);
	float yvec = 0;
	//写出CELL数组
	for (i = 0; i<deminfo.m_nHeight; i++)
	{
		for (j = 0; j<deminfo.m_nWidth - 1; j++)
		{
			yvec = m_p2DCellArray[i][j].XUnitFlux_Old / m_p2DCellArray[i][j].WaterHeight_Old;
			if (yvec == 0.0f) fout << 0 << " ";
			else fout << yvec << " ";
		}
		yvec = m_p2DCellArray[i][j].XUnitFlux_Old / m_p2DCellArray[i][j].WaterHeight_Old;
		if (yvec == 0.0f) fout << 0 << endl;
		else fout << yvec << endl;
	}

	fout.close();
	cout << "保存完毕!!" << endl;
}
void CAFlood::ExportAVec(char* strDataPath){

	int i, j;

	ofstream fout(strDataPath);
	//文件头的写入
	//fout<<"ncols         "<<deminfo.m_nWidth<<endl;
	//fout<<"nrows         "<<deminfo.m_nHeight<<endl;
	//fout<<"xllcorner     "<<deminfo.m_xllcorner<<endl;
	//fout<<"yllcorner     "<<deminfo.m_yllcorner<<endl;
	//fout<<"cellsize           "<<deminfo.m_fCellsize<<endl;
	//fout<<"NODATA_value  -9999"<<endl;
	ExportFileHeader(fout);
	fout << fixed << setprecision(2);
	float yvec = 0;
	float xvec = 0;
	float avec = 0;
	//写出CELL数组
	for (i = 0; i<deminfo.m_nHeight; i++)
	{
		for (j = 0; j<deminfo.m_nWidth - 1; j++)
		{
			if (m_p2DCellArray[i][j].WaterHeight_Old == 0){
				yvec = 0;
				xvec = 0;
			
			}
			else{
				yvec = m_p2DCellArray[i][j].YUnitFlux_Old / (m_p2DCellArray[i][j].WaterHeight_Old);
				xvec = m_p2DCellArray[i][j].XUnitFlux_Old / (m_p2DCellArray[i][j].WaterHeight_Old);
				
			}
			avec = sqrt((yvec*yvec) + (xvec*xvec)) * 40;
			if (avec == 0.0f) fout << 0 << " ";
			else fout << avec << " ";
		}


		if (m_p2DCellArray[i][j].WaterHeight_Old == 0){
			yvec = 0;
			xvec = 0;

		}
		else{
			yvec = m_p2DCellArray[i][j].YUnitFlux_Old / (m_p2DCellArray[i][j].WaterHeight_Old);
			xvec = m_p2DCellArray[i][j].XUnitFlux_Old / (m_p2DCellArray[i][j].WaterHeight_Old);

		}
		avec = sqrt((yvec*yvec) + (xvec*xvec))*40;
		if (avec == 0.0f) fout << 0 << endl;
		else fout << avec << endl;
	}

	fout.close();
	cout << "保存完毕!!" << endl;

}

void CAFlood::ExportFileHeader(ofstream &fout)
{
	//设置输出精度
	fout << fixed << setprecision(6);
	
	//文件头的写入
	fout << "ncols         " << deminfo.m_nWidth << endl;
	fout << "nrows         " << deminfo.m_nHeight << endl;
	fout << "xllcorner     " << deminfo.m_xllcorner << endl;
	fout << "yllcorner     " << deminfo.m_yllcorner << endl;
	fout << "cellsize           " << deminfo.m_fCellsize << endl;
	fout << "NODATA_value  -9999" << endl;
}
//监测水面是否继续变化
bool CAFlood::IsWaterSurfaceStable()
{
	double threshold = 0.001;  // 水面高度变化的阈值
	for (int i = 0; i < deminfo.m_nHeight; ++i)
	{
		for (int j = 0; j < deminfo.m_nWidth; ++j)
		{
			if (fabs(m_p2DCellArray[i][j].WaterHeight_New - m_p2DCellArray[i][j].WaterHeight_Old) > threshold)
			{
				return false;  // 只要有一个单元的水面高度变化超过阈值，就认为还没有稳定
			}
		}
	}
	return true;  // 所有单元的水面高度变化都在阈值内，认为水面已经稳定
}
