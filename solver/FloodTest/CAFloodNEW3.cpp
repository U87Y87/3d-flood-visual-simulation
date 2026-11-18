// CAFloodNEW.cpp : 定义控制台应用程序的入口点。
//
#include "stdafx.h"
#include "CAFloodNEW3.h"
using namespace std;
using namespace FLOOD3;

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
//#define new DEBUG_NEW
#endif

CAFlood::CAFlood()
{
	m_p2DCellArray=NULL;
	m_pOriginArray= NULL;
	m_nCount=0;
	m_fSpatialTimeRatio=100;
	m_fTimeUnit=0;
	m_fSumQ=0.0;
}

CAFlood::~CAFlood()
{
	if (m_p2DCellArray!=NULL)
	{
		for (int i=0;i<deminfo.m_nHeight;i++)
		{
			delete []m_p2DCellArray[i];
			m_p2DCellArray[i]=NULL;
		}
		delete []m_p2DCellArray;
		m_p2DCellArray=NULL;
	}
	if (m_pOriginArray!=NULL)
	{
		delete []m_pOriginArray;
		m_pOriginArray=NULL;
	}
}

void CAFlood::SetDamBreakInfo(float fDamVolume,float fDamWidth,float fDameHeight, float fBreakWidth,float fBreakHeight,bool bFullBreak,float fTypeFactor)
{
	daminfo.m_fDamVolume= fDamVolume;
	daminfo.m_fDamWidth= fDamWidth;
	daminfo.m_fDamHeight= fDameHeight;
	daminfo.m_fBreakHeight= fBreakHeight;
	daminfo.m_fBreakWidth= fBreakWidth;
	this->m_fTypeFactor=m_fTypeFactor;	
	m_bFullBreak= bFullBreak;
}
void CAFlood::SetDamBreakInfo(DAMINFORMATION daminfo,float m_fTypeFactor,bool bFullBreak)
{
	this->daminfo.m_fDamVolume= daminfo.m_fDamVolume;
	this->daminfo.m_fDamWidth= daminfo.m_fDamWidth;
	this->daminfo.m_fDamHeight= daminfo.m_fDamHeight;
	this->daminfo.m_fBreakHeight= daminfo.m_fBreakHeight;
	this->daminfo.m_fBreakWidth= daminfo.m_fBreakWidth;
	this->m_fTypeFactor=m_fTypeFactor;	
	m_bFullBreak= bFullBreak;
}

void CAFlood::ComputeTempFluxANDDepth(float t)
{
	float FluxTemp=0;
/*瞬时流量计算*/
	//因为拟合曲线时存在点位误差，头尾两点不能照顾全，做一个映射，让头尾两点到正确的位置
	double k0=m_fCoefficiant[5];
	double k1=m_fCoefficiant[0]*pow(m_fBreakTime,5)+m_fCoefficiant[1]*pow(m_fBreakTime,4)+m_fCoefficiant[2]*pow(m_fBreakTime,3)+m_fCoefficiant[3]*pow(m_fBreakTime,2)+m_fCoefficiant[4]*m_fBreakTime+m_fCoefficiant[5];
	double kx=m_fCoefficiant[0]*pow(t,5)+m_fCoefficiant[1]*pow(t,4)+m_fCoefficiant[2]*pow(t,3)+m_fCoefficiant[3]*pow(t,2)+m_fCoefficiant[4]*t+m_fCoefficiant[5];
	double kt=1+(kx-k0)/(k0-k1);
	m_fDamWaterFlux=kt*m_fMaxFlux;
	//控制小数的位数
	//保留两位小数则乘100，n位乘10的n次方，加0.5为四舍五入
	m_fDamWaterFlux=(double)((int)(m_fDamWaterFlux*100+0.5))/100.0;//瞬时流量输出
	
/*瞬时水位计算*/
	//库容水位曲线关系y = 70.243x3 - 784x2 + 8304.5x

	m_fRestVolume=m_fRealDamVolume-Integral((int)t,m_fCoefficiant)*m_fMaxFlux;//剩余库容
	if (m_fRestVolume<0)
	{
		m_fRestVolume=0;
	}
	double depth;//计算水深
	depth=sanci(70.243,-784,8304.5,-m_fRestVolume);
	double depth0=sanci(70.243,-784,8304.5,-m_fRealDamVolume);
	double depth1=0;
	m_fDamWaterHeight=61.5*(depth-depth1)/(depth0-depth1);//调整计算误差，使整个数据落在0-100米范围内
	if (m_fDamWaterHeight<0)
	{
		m_fDamWaterHeight=0;
	}
	//控制小数的位数
	m_fDamWaterHeight=(double)((int)(m_fDamWaterHeight*10+0.5))/10.0+725.8;//瞬时水位
	return;
}

double CAFlood::Integral(int y,double k[6])
{
	//求以k[6]数组六个元素为多项式系数的五次多项式的0-y范围内的积分
	double z;
	z=k[0]*pow((double)y,6)/6.0+k[1]*pow((double)y,5)/5.0+k[2]*pow((double)y,4)/4.0+k[3]*pow((double)y,3)/3.0+k[4]*pow((double)y,2)/2.0+k[5]*y;
	return z;
}

double CAFlood::sanci(double a,double b,long double c,long double d)
{
	double  td=0.333333;

	//盛金公式求三次方程的根：
	//本例根据曲线，适用于盛金公式中一实根情形
	double x;
	double A=pow(b,2)-3*a*c;
	double B=b*c-9*a*d;
	double C=pow(c,2)-3*b*d;
	double derta=pow(B,2)-4*A*C;
	double y1,y2,x1,x2;
	y1=A*b+3*a*(-B+pow((pow(B,2)-4*A*C),0.5))/2.0;
	y2=A*b+3*a*(-B-pow((pow(B,2)-4*A*C),0.5))/2.0;
	if (y1<0)
	{
		x1=-pow(-y1,td);
	} 
	else
	{
		x1=pow(y1,td);
	}
	if (y2<0)
	{
		x2=-pow(-y2,td);
	} 
	else
	{
		x2=pow(y2,td);
	}
	x=(-b-x1-x2)/(3*a);

	return x;
}

void CAFlood::ComputeRushTimeANDQm()
{
	//计算峰值流量
	double lamda,Qm;
	float fRealDamVolume;//实际溃出库容
	//不同的溃决方式只是在计算最大库容和最大流量上有差别
	if (m_bFullBreak)
	{
		m_fRealDamVolume= daminfo.m_fDamVolume;
		double temp=2*pow((double)m_fTypeFactor,0.5)/(double)(1+2*(double)m_fTypeFactor);
		lamda=pow((double)m_fTypeFactor,(double)m_fTypeFactor-1)*pow((double)temp,(double)2*(double)m_fTypeFactor+1);
		Qm=lamda*(double)daminfo.m_fDamWidth*pow(g,0.5)*pow((double)daminfo.m_fDamHeight,1.5);
	}
	else
	{
		double h=daminfo.m_fDamHeight-daminfo.m_fBreakHeight;
		if (m_fTypeFactor==1) //对矩形情况下进行简化分析，这样貌似准确点
		{
			m_fRealDamVolume= daminfo.m_fDamVolume* daminfo.m_fBreakHeight/daminfo.m_fDamHeight;
		}
		else
		{
			m_fRealDamVolume =daminfo.m_fDamVolume-70.243*pow(h,3)+784*pow(h,2)-8304.5*h;
		}

		Qm=(8.0/27.0)*pow((double)g,0.5)*pow((double)daminfo.m_fDamWidth/(double)daminfo.m_fBreakWidth,0.4)*pow((double)(11*daminfo.m_fDamHeight-10*daminfo.m_fBreakHeight)/(double)daminfo.m_fDamHeight,0.3)*daminfo.m_fBreakWidth*pow((double)daminfo.m_fBreakHeight,1.5);
		
	}
	//控制小数的位数
	//保留两位小数则乘100，n位乘10的n次方，加0.5为四舍五入
	m_fMaxFlux=(double)((int)(Qm*100+0.5))/100.0;
	//计算泄空时间
	long tx1=0,tx2;
	double T0=4*m_fRealDamVolume/(double)m_fMaxFlux;
	double dertaW=m_fRealDamVolume;
	double test1;
	while(dertaW>0)
	{
		m_fCoefficiant[0]=-19.565/(double)pow(T0,5);
		m_fCoefficiant[1]=56.595/(double)pow(T0,4);
		m_fCoefficiant[2]=-61.153/(double)pow(T0,3);
		m_fCoefficiant[3]=30.574/(double)pow(T0,2);
		m_fCoefficiant[4]=-7.43/(double)T0;
		m_fCoefficiant[5]=0.9723;
		do 
		{			
			tx2=tx1;
			tx1=tx1+100;	
		} 
		while (Integral(tx2,m_fCoefficiant)<Integral(tx1,m_fCoefficiant));
		int tips100=tx2;
		tx1=tx2;
		do 
		{
			tx2=tx1;
			tx1=tx1+10;
		} while (Integral(tx2,m_fCoefficiant)<Integral(tx1,m_fCoefficiant)&&tx1<(tips100+100));
		int tips10=tx2;
		tx1=tx2;
		do 
		{
			tx2=tx1;
			tx1=tx1+1;
		} while (Integral(tx2,m_fCoefficiant)<Integral(tx1,m_fCoefficiant)&&tx1<(tips10+10));
		T0=double(tx2);
		test1=Integral(tx2,m_fCoefficiant)*Qm;
        dertaW=m_fRealDamVolume-test1;
		
	};//计算泄空时间，调整T0的值，使得到达泄空时间时库容为0
	m_fBreakTime=T0;	
	return;
}

bool CAFlood::InitCellArray(char* strDataDir)
{
	int nWidth,nHeight;
	int i,j;
	float fCellsize;
	float* pDemdata= NULL;
	
	char strDemPath[80];
	char strCoarseRate[80];
	char strRiverInit[80];
	strcpy(strDemPath,strDataDir);
	strcpy(strCoarseRate,strDataDir);
	strcpy(strRiverInit,strDataDir);

	strcat(strDemPath,"\\dem.asc");
	strcat(strCoarseRate,"\\cao.asc");
	strcat(strRiverInit,"\\InputRiver.dat");
/*读进DEM数据,分配cell数组*/
	FILE * file= fopen(strDemPath,"r");
	if (file==NULL) 
	{
		//AfxMessageBox("打开DEM文件失败!");
		cerr<<"打开DEM文件失败!";
		return false;
	}
	long nCount;
    fseek(file, 0L, SEEK_END);
    nCount = ftell(file);
    fseek(file, 0L, SEEK_SET);	
	char *pData= new char[nCount];
	fread(pData,1,nCount,file);
//读文件头
	strtok(pData," \n");
	nWidth= atoi(strtok(NULL," \n"));
	strtok(NULL," \n");
	nHeight= atoi(strtok(NULL," \n"));
	strtok(NULL," \n");
	deminfo.m_xllcorner= atof(strtok(NULL," \n"));
	strtok(NULL," \n");
	deminfo.m_yllcorner= atof(strtok(NULL," \n"));
	strtok(NULL," \n");
	fCellsize= atof(strtok(NULL," \n"));
	strtok(NULL,"\n");
	deminfo.m_nWidth= nWidth;
	deminfo.m_nHeight=nHeight;
	deminfo.m_fCellsize= fCellsize;
	m_fTimeUnit= deminfo.m_fCellsize/m_fSpatialTimeRatio;
//分配CELL数组内存，初始化索引数组和部分CELL数组
	float temp=0;
	RIVERINDEX index;
	m_p2DCellArray= new CACELL* [nHeight];
	for (i=0;i<nHeight;i++)
	{
		m_p2DCellArray[i]= new CACELL[nWidth];
	}

	for (i=0;i<nHeight;i++)
		for ( j=0;j<nWidth;j++)
		{
			temp= atof(strtok(NULL," \n"));
			if ((i==25)) 
			{
				int ttt=0;
			}
			if (temp!=9999)
			{
				index.x= j;
				index.y= i;
				m_vector.push_back(index);
			}

			if(temp!=9999){
            double X=this->deminfo.m_xllcorner+j*this->deminfo.m_fCellsize;
			double Y=this->deminfo.m_yllcorner+(nHeight-i)*this->deminfo.m_fCellsize; 
			UTM2LatLon* c = new UTM2LatLon();
			double  LatLong[2];

			stringstream ss;

			stringstream strX,strY;
			strX<<X;
			strY<<Y;			

			std::string s1=strX.str();  
			std::string s2=strY.str();  

			std::string ssss="48 N "+s1+" "+s2;

			c->convertUTMToLatLong(ssss,LatLong);

			m_p2DCellArray[i][j].Lat=LatLong[0]*3.1415926535897932384626/180; //转换为弧度
			m_p2DCellArray[i][j].Long=LatLong[1]*3.1415926535897932384626/180;//转换为弧度
			}else
			{
			m_p2DCellArray[i][j].Lat=0; //转换为弧度
			m_p2DCellArray[i][j].Long=0;//转换为弧度
			}

			m_p2DCellArray[i][j].DemData= temp;
			m_p2DCellArray[i][j].WaterHeight_New=0;
			m_p2DCellArray[i][j].WaterHeight_Old= temp;
			m_p2DCellArray[i][j].XUnitFlux_New=0;
			m_p2DCellArray[i][j].XUnitFlux_Old=0;
			m_p2DCellArray[i][j].YUnitFlux_New=0;
			m_p2DCellArray[i][j].YUnitFlux_Old=0;
			m_p2DCellArray[i][j].b_IsRiver=false;
			m_p2DCellArray[i][j].b_IsOrigin=false;
			m_p2DCellArray[i][j].b_IsEnd=false;
			m_p2DCellArray[i][j].vec= 0;
		}
//销毁临时内存，关闭文件
	delete []pData;
	fclose(file);

/*读入糙率和基流数据,初始化部分cell数组*/
//读入糙率数据,直接放在元胞数组里
	file= fopen(strCoarseRate,"r");
    fseek(file, 0L, SEEK_END);
    nCount = ftell(file);
    fseek(file, 0L, SEEK_SET);	
	pData= new char[nCount];
	fread(pData,1,nCount,file);
	strtok(pData,"\n");
	strtok(NULL,"\n");
	strtok(NULL,"\n");
	strtok(NULL,"\n");
	strtok(NULL,"\n");
	strtok(NULL,"\n");

	for (i=0;i<nHeight;i++)
		for(j=0;j<nWidth;j++)
		{
			if ((i==25)&&(j==10)) 
			{
				int ttt=0;
			}
			m_p2DCellArray[i][j].CoaseRate=atof(strtok(NULL," \n"));
		}
	delete []pData;
	fclose(file);
//读入基流数据,存入基流数组pRiArray中
	file= fopen(strRiverInit,"r");
    fseek(file, 0L, SEEK_END);
    nCount = ftell(file);
    fseek(file, 0L, SEEK_SET);	
	pData= new char[nCount];
	fread(pData,1,nCount,file);
	m_nCellNum= atoi(strtok(pData," \n"));
	m_pOriginArray= new RIVERINDEX[m_nCellNum];
	for (i=0;i<m_nCellNum;i++)
	{
		m_pOriginArray[i].x= atof(strtok(NULL," \n"));
		m_pOriginArray[i].y= atof(strtok(NULL," \n"));
	}
	delete []pData;
	fclose(file);
	int m,n=0;
	for (i=0;i<m_nCellNum;i++)
	{
		n= m_pOriginArray[i].x;
		m= m_pOriginArray[i].y;
		m_p2DCellArray[nHeight-m][n].b_IsRiver=true;
		m_p2DCellArray[nHeight-m][n].b_IsOrigin=true;
		m_p2DCellArray[nHeight-m][n].DemData=9999;
		m_p2DCellArray[deminfo.m_nHeight-m][n].WaterHeight_New= 9999;
		m_p2DCellArray[deminfo.m_nHeight-m][n].WaterHeight_Old= 9999;
	}
//对上下边界进行初始化
	for (i=0;i<deminfo.m_nWidth;i++)
	{
		m_p2DCellArray[0][i].DemData=9999;
		m_p2DCellArray[0][i].WaterHeight_New=9999;
		m_p2DCellArray[0][i].WaterHeight_Old=9999;
		m_p2DCellArray[deminfo.m_nHeight-1][i].DemData=9999;
		m_p2DCellArray[deminfo.m_nHeight-1][i].WaterHeight_New=9999;
		m_p2DCellArray[deminfo.m_nHeight-1][i].WaterHeight_Old=9999;
	}
//对左右边界进行初始化
	for (i=0;i<deminfo.m_nHeight;i++)
	{
		m_p2DCellArray[i][0].DemData=9999;
		m_p2DCellArray[i][0].WaterHeight_New=9999;
		m_p2DCellArray[i][0].WaterHeight_Old=9999;
		m_p2DCellArray[i][deminfo.m_nWidth-1].DemData=9999;
		m_p2DCellArray[i][deminfo.m_nWidth-1].WaterHeight_New=9999;
		m_p2DCellArray[i][deminfo.m_nWidth-1].WaterHeight_Old=9999;
	}

	m_fCurTime=0;    //当前时间归0

	return true;
}


void CAFlood::ComputeSourceFlood()
{
	int m,n=0;
	//int i,j;	
	//仅维护源头的流量，后面不考虑
	for (int i=0;i<m_nCellNum;i++)
	{
		n= m_pOriginArray[i].x;
		m= m_pOriginArray[i].y;
		m_p2DCellArray[deminfo.m_nHeight-m][n].XUnitFlux_Old= (m_fDamWaterFlux*2*m_fTimeUnit)/(2*m_nCellNum*deminfo.m_fCellsize*m_fTimeUnit);
		m_p2DCellArray[deminfo.m_nHeight-m][n].XUnitFlux_New= m_p2DCellArray[deminfo.m_nHeight-m][n].XUnitFlux_Old;
		m_p2DCellArray[deminfo.m_nHeight-m][n].YUnitFlux_Old= m_p2DCellArray[deminfo.m_nHeight-m][n].XUnitFlux_Old;
		m_p2DCellArray[deminfo.m_nHeight-m][n].YUnitFlux_New= m_p2DCellArray[deminfo.m_nHeight-m][n].XUnitFlux_Old;
	}
}

void CAFlood::ComputeNewFloodFromHeight()
{
	int i,j;
	int IndexCount;
	//根据旧水面高度计算新流速
	float fWaterDepth,fWaterDepthR,fWaterDepthU;
	float fAverWaterDepthX,fAverWaterDepthY;
	float fAverWaterHeightX;
	float fAverWaterHeightY;
	float fDiffWaterHeightX;
	float fDiffWaterHeightY;
	float xvec=0;
	float yvec=0;
	for (IndexCount=0;IndexCount<m_vector.size();IndexCount++)
	{
		i= m_vector[IndexCount].y;
		j= m_vector[IndexCount].x;
		if ((i==0)||(j==0)||(i==deminfo.m_nHeight-1)||(j==deminfo.m_nWidth-1))
		{
			continue;
		}
		if (m_p2DCellArray[i][j].b_IsOrigin==false) //维护非源头的新流量
		{			
			ComputeAverageWaterAndDiffHeight(i,j,&fWaterDepth,&fWaterDepthR,&fWaterDepthU,&fAverWaterDepthX,
				&fAverWaterDepthY,&fDiffWaterHeightX,&fDiffWaterHeightY);
			
			ComputeXFlux(fAverWaterDepthX, fWaterDepthR, fWaterDepth, fAverWaterDepthY,fDiffWaterHeightX, i, j,&xvec, &yvec);				
			
			ComputeYFlux(fAverWaterDepthY,fWaterDepthU,fWaterDepth,fAverWaterDepthX,fDiffWaterHeightY,i, j,&xvec, &yvec);

			RepairFlux(i,j);
			m_p2DCellArray[i][j].vec= sqrt(xvec*xvec+yvec*yvec);
		}
	}
}

void CAFlood::ComputeAverageWaterAndDiffHeight(int i, int j, float *fWaterDepth, float *fWaterDepthR, float *fWaterDepthU, 
											   float *fAverWaterDepthX, float *fAverWaterDepthY, float *fDiffWaterHeightX, float *fDiffWaterHeightY)
{
	*fWaterDepth= m_p2DCellArray[i][j].WaterHeight_Old- m_p2DCellArray[i][j].DemData;//计算当前格网水深
	*fWaterDepthR= m_p2DCellArray[i][j+1].WaterHeight_Old- m_p2DCellArray[i][j+1].DemData;//计算右边格网水深
	*fWaterDepthU= m_p2DCellArray[i+1][j].WaterHeight_Old- m_p2DCellArray[i+1][j].DemData;//计算下边格网水深
	*fAverWaterDepthX= 0.5*(*fWaterDepth+*fWaterDepthR);//和右边格网的平均水深
	*fAverWaterDepthY= 0.5*(*fWaterDepth+*fWaterDepthU);//和下边格网的平均水深
	//水位高差，fDiffWaterHeightX为当前格比右边高出的部分，fDiffWaterHeightY当前格比下面高出的
	*fDiffWaterHeightX= m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i][j+1].WaterHeight_Old;
	*fDiffWaterHeightY= m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i+1][j].WaterHeight_Old;
}

void CAFlood::ComputeXFlux(float fAverWaterDepthX, float fWaterDepthR, float fWaterDepth, float fAverWaterDepthY,
				  float fDiffWaterHeightX, int i, int j,float *xvec, float *yvec)
{
	float temp1,temp2,temp3; //单宽流量变化率计算公式中的临时量
	/*x方向新单宽流量*/
	//如果一个格子的水面高度低于另一个格子的地面，且另一个格子没有水，或者两个格子都没有水，则不发生流动
	if ((fAverWaterDepthX<=zero)||
		((m_p2DCellArray[i][j].WaterHeight_Old<m_p2DCellArray[i][j+1].DemData)&&fWaterDepthR<=zero)||
		((m_p2DCellArray[i][j+1].WaterHeight_Old<m_p2DCellArray[i][j].DemData)&&fWaterDepth<=zero))
	{
		m_p2DCellArray[i][j].XUnitFlux_New= 0;
	}
	else
	{
		//根据旧的流量算速度(单位:米/每秒)
		*xvec= m_p2DCellArray[i][j].XUnitFlux_Old/fAverWaterDepthX;
		if (fabs(*xvec)<zero)
			temp2=0;
		else
		{
			if (fAverWaterDepthY<=zero) 
				*yvec=0;
			else
				//
				*yvec= m_p2DCellArray[i][j].YUnitFlux_Old/fAverWaterDepthY;
			temp1= pow((float)fAverWaterDepthX,(float)(1.0/3.0));
			//	temp2= 2*g*dt*m_p2DCellArray[i][j].CoaseRate*xvec*sqrt(xvec*xvec+yvec*yvec)/temp1;
			temp2= g*m_fTimeUnit*m_p2DCellArray[i][j].CoaseRate*m_p2DCellArray[i][j].CoaseRate*
				*xvec*sqrt(*xvec**xvec+*yvec**yvec)/temp1;
			if (fabs(m_p2DCellArray[i][j].XUnitFlux_Old)<fabs(temp2)) 
				temp2= m_p2DCellArray[i][j].XUnitFlux_Old;
		}
		temp3=g*m_fTimeUnit*fAverWaterDepthX*fDiffWaterHeightX/deminfo.m_fCellsize;
		m_p2DCellArray[i][j].XUnitFlux_New=m_p2DCellArray[i][j].XUnitFlux_Old-(temp2-temp3);
	}

}
void CAFlood::ComputeYFlux(float fAverWaterDepthY,float fWaterDepthU,float fWaterDepth,float fAverWaterDepthX,float fDiffWaterHeightY,int i, int j,float *xvec, float *yvec)
{
	float temp1,temp2,temp3; //单宽流量变化率计算公式中的临时量
	//y方向新单宽流量
			if ((fAverWaterDepthY<=zero)||
				((m_p2DCellArray[i][j].WaterHeight_Old<m_p2DCellArray[i+1][j].DemData)&&fWaterDepthU<=zero)||
				((m_p2DCellArray[i+1][j].WaterHeight_Old<m_p2DCellArray[i][j].DemData)&&fWaterDepth<=zero))
			{
				m_p2DCellArray[i][j].YUnitFlux_New= 0;
			}
			else
			{
				*yvec= m_p2DCellArray[i][j].YUnitFlux_Old/fAverWaterDepthY;
				if (fabs(*yvec)<zero)
					temp2=0;
				else
				{
					if (fAverWaterDepthX<=zero) 
						*xvec=0;
					else
						*xvec= m_p2DCellArray[i][j].XUnitFlux_Old/fAverWaterDepthX;
					temp1= pow((float)fAverWaterDepthY,(float)(1.0/3.0));
					//temp2= 2*g*dt*m_p2DCellArray[i][j].CoaseRate*yvec*sqrt(xvec*xvec+yvec*yvec)/temp1;
					temp2= g*m_fTimeUnit*m_p2DCellArray[i][j].CoaseRate*m_p2DCellArray[i][j].CoaseRate*
						*yvec*sqrt(*xvec**xvec+*yvec**yvec)/temp1;
					if (fabs(m_p2DCellArray[i][j].YUnitFlux_Old)<fabs(temp2)) 
						temp2= m_p2DCellArray[i][j].YUnitFlux_Old;
				}
				temp3=g*m_fTimeUnit*fAverWaterDepthY*fDiffWaterHeightY/deminfo.m_fCellsize;
				m_p2DCellArray[i][j].YUnitFlux_New=m_p2DCellArray[i][j].YUnitFlux_Old-(temp2-temp3);
				
			}
}

void CAFlood::ComputeNewheightFromFlux(float *dH,float *fSumH)
{
	//float fSumH=0;
	int i,j;
	int IndexCount;
	//根据新流速计算新的水面高度
	for (IndexCount=0;IndexCount<m_vector.size();IndexCount++)
	{
		i= m_vector[IndexCount].y;
		j= m_vector[IndexCount].x;
		//这里需要对源头的背面进行严格的界定
		if ((i==27)&&(j==15)&&(m_fCurTime>6.7))
		{
			int ttt=0;
		}
		if ((i==0)||(j==0)||(i==deminfo.m_nHeight-1)||(j==deminfo.m_nWidth-1))
		{
			continue;
		}
		//维护非源头的新水位
		if (m_p2DCellArray[i][j].b_IsOrigin==false)
		{
			*dH= (m_p2DCellArray[i][j-1].XUnitFlux_New-m_p2DCellArray[i][j].XUnitFlux_New+
				m_p2DCellArray[i-1][j].YUnitFlux_New-m_p2DCellArray[i][j].YUnitFlux_New)*m_fTimeUnit/deminfo.m_fCellsize;
			m_p2DCellArray[i][j].WaterHeight_New= m_p2DCellArray[i][j].WaterHeight_Old+ *dH;
			
			if (m_p2DCellArray[i][j].WaterHeight_New<=m_p2DCellArray[i][j].DemData) 
			{
				m_p2DCellArray[i][j].WaterHeight_New=m_p2DCellArray[i][j].DemData;
				m_p2DCellArray[i][j].b_IsRiver=false;
			}
			else
				m_p2DCellArray[i][j].b_IsRiver=true;
//特殊处理			
			KillPots(i,j);
			if ((m_fSumQ> m_fRealDamVolume+6000 ) &&(m_lRepairNum>0))
			{			
				RepairVolume(i, j);
			}
//特殊处理
			m_p2DCellArray[i][j].WaterHeight_Old= m_p2DCellArray[i][j].WaterHeight_New;
			m_p2DCellArray[i][j].XUnitFlux_Old=m_p2DCellArray[i][j].XUnitFlux_New;
			m_p2DCellArray[i][j].YUnitFlux_Old=m_p2DCellArray[i][j].YUnitFlux_New;
		}
		*fSumH+= m_p2DCellArray[i][j].WaterHeight_New-m_p2DCellArray[i][j].DemData;
	}
}

void CAFlood::Calcultate()
{
//重新计算溃口的水高和流量
	if (m_fCurTime<=m_fBreakTime) 
	{
		ComputeTempFluxANDDepth(m_fCurTime);
	}
//初始化CELL数组时要经过点处理，m_fDamWaterFlux为t时刻1S内所有格网的流量总和
//当前所获得的是2*m_fTimeUnit流经每个格子每个方向上的流量
	int i,j;	
	////仅维护源头的流量，后面不考虑
	ComputeSourceFlood();

	//根据旧水面高度计算新流速
	float fWaterDepth,fWaterDepthR,fWaterDepthU;
	float fAverWaterDepthX,fAverWaterDepthY;
	float fAverWaterHeightX;
	float fAverWaterHeightY;
	float fDiffWaterHeightX;
	float fDiffWaterHeightY;
	float xvec=0;
	float yvec=0;
	m_nCount++;
	float temp1,temp2,temp3; //单宽流量变化率计算公式中的临时量
	int IndexCount;
		
	ComputeNewFloodFromHeight();

	float dH;
	float fSumH=0;	

	ComputeNewheightFromFlux(&dH,&fSumH);

	m_fCurTime+= 2*m_fTimeUnit; 
	m_fSumQ= fSumH* deminfo.m_fCellsize* deminfo.m_fCellsize+ m_fRestVolume;
	
	m_lRepairNum= 12000/(deminfo.m_fCellsize* deminfo.m_fCellsize);
}

void CAFlood::RepairVolume(long i,long j)
{
	if ((m_p2DCellArray[i][j].WaterHeight_New-m_p2DCellArray[i][j].DemData)>5 ) 
	{
		m_p2DCellArray[i][j].WaterHeight_New =m_p2DCellArray[i][j].WaterHeight_New-0.5;
		m_lRepairNum--;
	}
}
void CAFlood::RepairFlux(long i,long j)
{
	//这里要加上一个判断,修正过大的流量
	if ((m_p2DCellArray[i][j].YUnitFlux_New+m_p2DCellArray[i][j].XUnitFlux_New)*m_fTimeUnit>
		(m_p2DCellArray[i][j].WaterHeight_Old- m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize) 
	{
		float Q= (m_p2DCellArray[i][j].WaterHeight_Old- m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize/m_fTimeUnit;
		float sum= (m_p2DCellArray[i][j].YUnitFlux_New+m_p2DCellArray[i][j].XUnitFlux_New);
		m_p2DCellArray[i][j].YUnitFlux_New= Q* m_p2DCellArray[i][j].YUnitFlux_New/sum;
		m_p2DCellArray[i][j].XUnitFlux_New= Q* m_p2DCellArray[i][j].XUnitFlux_New/sum;
	}
	//		//反方向修正
	if (-(m_p2DCellArray[i-1][j].YUnitFlux_New+m_p2DCellArray[i][j-1].XUnitFlux_New)*m_fTimeUnit>
		(m_p2DCellArray[i][j].WaterHeight_Old- m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize) 
	{
		float Q= (m_p2DCellArray[i][j].WaterHeight_Old- m_p2DCellArray[i][j].DemData)*deminfo.m_fCellsize/m_fTimeUnit;
		float sum= (m_p2DCellArray[i][j-1].XUnitFlux_New+m_p2DCellArray[i-1][j].YUnitFlux_New);
		m_p2DCellArray[i-1][j].YUnitFlux_New= Q* m_p2DCellArray[i-1][j].YUnitFlux_New/sum;
		m_p2DCellArray[i][j-1].XUnitFlux_New= Q* m_p2DCellArray[i][j-1].XUnitFlux_New/sum;
		
	}	

}
void CAFlood::KillPots(long i,long j)
{
	if ((i==35)&&(j==11)&&(m_fCurTime>27))
	{
		int ttt=0;
	}
	float LeftDepth=m_p2DCellArray[i][j-1].WaterHeight_New-m_p2DCellArray[i][j-1].DemData;
	float RightDepth=m_p2DCellArray[i][j+1].WaterHeight_New-m_p2DCellArray[i][j+1].DemData;
	float UpperDepth=m_p2DCellArray[i+1][j].WaterHeight_New-m_p2DCellArray[i+1][j].DemData;
	float LowerDepth=m_p2DCellArray[i-1][j].WaterHeight_New-m_p2DCellArray[i-1][j].DemData;
	float LeftLowerDepth= m_p2DCellArray[i-1][j-1].WaterHeight_New-m_p2DCellArray[i-1][j-1].DemData;
	float LeftUpperDepth= m_p2DCellArray[i+1][j-1].WaterHeight_New-m_p2DCellArray[i+1][j-1].DemData;
	float RightLowerDepth= m_p2DCellArray[i-1][j+1].WaterHeight_New-m_p2DCellArray[i-1][j+1].DemData;
	float RightUpperDepth= m_p2DCellArray[i+1][j+1].WaterHeight_New-m_p2DCellArray[i+1][j+1].DemData;
	float Depth= m_p2DCellArray[i][j].WaterHeight_New-m_p2DCellArray[i][j].DemData;
	int nCount=0;
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

	if (m_fCurTime>10)
	{
		if ((nCount<=2)&&(Depth>0)&&((LeftDepth+RightDepth+UpperDepth+LowerDepth+LeftLowerDepth+LeftUpperDepth+RightLowerDepth+RightUpperDepth)<1))
		{
			m_p2DCellArray[i][j].WaterHeight_New= 	m_p2DCellArray[i][j].DemData;
			m_p2DCellArray[i][j].b_IsRiver= false;
		}
	}
	if ((nCount>=7)&&(Depth==0))
	{
		m_p2DCellArray[i][j].WaterHeight_New= 	m_p2DCellArray[i][j].DemData+ 0.1;
		m_p2DCellArray[i][j].b_IsRiver= true;
	}
}


void CAFlood::ExportDepth(char* strDataPath)
{
	int i,j;
	//if (_access( strDataPath,0 ) != -1) 
	//{
	//	DeleteFile(strDataPath) ;
	//}
	ofstream fout(strDataPath,ios::out);

//文件头的写入
	ExportFileHeader(fout);
	//写出CELL数组
	for (i=0;i<deminfo.m_nHeight;i++)
	{
		for (j=0;j<deminfo.m_nWidth-1;j++)
		{
			fout<<m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i][j].DemData<<" ";
		}
		fout<<m_p2DCellArray[i][j].WaterHeight_Old-m_p2DCellArray[i][j].DemData<<endl;
	}

	fout.close();
	cout<<"保存完毕!!"<<endl;
	
}

void CAFlood::ExportXVec(char* strDataPath)
{
	int i,j;
	//if (_access( strDataPath,0 ) != -1) 
	//{
	//	DeleteFile(strDataPath) ;
	//}
	ofstream fout(strDataPath);
//文件头的写入
	ExportFileHeader(fout);
	float xvec=0;
	//写出CELL数组
	for (i=0;i<deminfo.m_nHeight;i++)
	{
		for (j=0;j<deminfo.m_nWidth-1;j++)
		{
			xvec= m_p2DCellArray[i][j].XUnitFlux_Old/m_p2DCellArray[i][j].WaterHeight_Old;
			fout<<xvec<<" ";
		}
		xvec= m_p2DCellArray[i][j].XUnitFlux_Old/m_p2DCellArray[i][j].WaterHeight_Old;
		fout<<xvec<<endl;
	}

	fout.close();
	cout<<"保存完毕!!"<<endl;
}

void CAFlood::ExportYVec(char* strDataPath)
{
	int i,j;
	//if (_access( strDataPath,0 ) != -1) 
	//{
	//	DeleteFile(strDataPath) ;
	//}
	ofstream fout(strDataPath);
//文件头的写入
	ExportFileHeader(fout);
	float yvec=0;
	//写出CELL数组
	for (i=0;i<deminfo.m_nHeight;i++)
	{
		for (j=0;j<deminfo.m_nWidth-1;j++)
		{
			yvec= m_p2DCellArray[i][j].XUnitFlux_Old/m_p2DCellArray[i][j].WaterHeight_Old;
			fout<<yvec<<" ";
		}
		yvec= m_p2DCellArray[i][j].XUnitFlux_Old/m_p2DCellArray[i][j].WaterHeight_Old;
		fout<<yvec<<endl;
	}

	fout.close();
	cout<<"保存完毕!!"<<endl;	
}

void CAFlood::ExportFileHeader(ofstream &fout)
{
	//文件头的写入
	fout<<"ncols         "<<deminfo.m_nWidth<<endl;
	fout<<"nrows         "<<deminfo.m_nHeight<<endl;
	fout<<"xllcorner     "<<deminfo.m_xllcorner<<endl;
	fout<<"yllcorner     "<<deminfo.m_yllcorner<<endl;
	fout<<"cellsize           "<<deminfo.m_fCellsize<<endl;
	fout<<"NODATA_value  -9999"<<endl;
}