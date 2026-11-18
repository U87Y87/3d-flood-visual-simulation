#include"stdafx.h"
//#include"CAFloodNEW4.h"
//#include<string>
//using namespace FLOOD4;
//bool InitCellArray(char* strDataDir);
//bool OpenFile(char *strPath,FILE * file);
//void ReadDEMFileHead(FILE * file,int *nWidth,int *nHeight,float *fCellsize, int *xllCorner, int *yllCorner,char *pData);
//
//void SetDamBreakInfo(bool m_bFullBreak,float m_fTypeFactor,float m_fDamVolume,float m_fDamWidth, 
//					 float m_fDamHeight,float m_fBreakWidth,float m_fBreakHeight,DAMINFORMATION *daminfo)
//{
//	daminfo->m_bFullBreak=m_bFullBreak;
//	daminfo->m_fTypeFactor=m_fTypeFactor;
//	daminfo->m_fDamVolume=m_fDamVolume;
//	daminfo->m_fDamWidth=m_fDamWidth;
//	daminfo->m_fDamHeight=m_fDamHeight;
//	daminfo->m_fBreakWidth=m_fBreakWidth;
//	daminfo->m_fBreakHeight=m_fBreakHeight;
//}
//void VirtualMain()
//{
//	string strdatapath="D:\\FloodProgramVS2005\\osg2.8.2\\FloodData\\20m";
//	InitCellArray((char *)strdatapath.c_str());	
//
//	int m_fSpatialTimeRatio= 100;       //设置时空比率
//	/*设置溃坝信息*/ 
//	DAMINFORMATION dam;
//	dam.m_fDamVolume=14000000;
//	dam.m_fDamWidth=133;
//	dam.m_fDamHeight=100;
//	dam.m_fBreakWidth=133;
//	dam.m_fBreakHeight=30.75;
//	float typafact=1.0;
//	bool bFullBreak=true;
//	//ComputeRushTimeANDQm();
//	//SetDamBreakInfo(true,fTypeFactor,DamVolume,DamWidth,DamHeight,BreakWidth,BreakHeight,&daminfo);	
//}
//
//bool InitCellArray(char* strDataDir)
//{
//	int nWidth,nHeight;
//	int xllCorner, yllCorner;
//	int i,j;
//	float fCellsize;    //
//	float fTimeUnit;    //每次循环的时空比
//	float fSpatialTimeRatio; //时空比
//	CACELL** p2DCellArray;
//
//	char strDemPath[80];
//	char strCoarseRate[80];
//	char strRiverInit[80];
//	strcpy(strDemPath,strDataDir);
//	strcpy(strCoarseRate,strDataDir);
//	strcpy(strRiverInit,strDataDir);
//
//	strcat(strDemPath,"\\dem.asc");
//	strcat(strCoarseRate,"\\cao.asc");
//	strcat(strRiverInit,"\\InputRiver.dat");
//	FILE * file;
//	char *pData;
//	if(!OpenFile(strDemPath,file))
//	{
//		return false;
//	}
//	ReadDEMFileHead(file, &nWidth,&nHeight,&fCellsize, &xllCorner, &yllCorner,pData);
//	fTimeUnit= fCellsize/fSpatialTimeRatio;             //设置时空比
//
//	//分配CELL数组内存，初始化索引数组和部分CELL数组
//	float temp=0;
//	RIVERINDEX index;
//	p2DCellArray= new CACELL* [nHeight];
//	for (i=0;i<nHeight;i++)
//	{
//		p2DCellArray[i]= new CACELL[nWidth];
//	}
//}
//
//bool OpenFile(char *strPath,FILE * file)
//{
//	file= fopen(strPath,"r");
//	if (file==NULL) 
//	{
//		//AfxMessageBox("打开DEM文件失败!");
//		cerr<<"打开文件失败!";
//		return false;
//	}
//	return true;
//}
//
//
//void ReadDEMFileHead(FILE * file,int *nWidth,int *nHeight,float *fCellsize, int *xllCorner, int *yllCorner,char *pData)
//{
//	long nCount;
//    fseek(file, 0L, SEEK_END);
//    nCount = ftell(file);
//    fseek(file, 0L, SEEK_SET);	
//	pData= new char[nCount];
//	fread(pData,1,nCount,file);
//	//读文件头
//	strtok(pData," \n");
//	*nWidth= atoi(strtok(NULL," \n"));
//	strtok(NULL," \n");
//	*nHeight= atoi(strtok(NULL," \n"));
//	strtok(NULL," \n");
//	*xllCorner= atof(strtok(NULL," \n"));
//	strtok(NULL," \n");
//	*yllCorner= atof(strtok(NULL," \n"));
//	strtok(NULL," \n");
//	*fCellsize= atof(strtok(NULL," \n"));
//	strtok(NULL,"\n");
//}
//
//void SetFloodCellInfo(int nHeight, int nWidth,	int xllCorner, int yllCorner, int fCellsize, CACELL** p2DCellArray, FILE * file,char *pData)
//{
//	float temp=0;
//	for (int i=0;i<nHeight;i++)
//		for (int j=0;j<nWidth;j++)
//		{
//			temp= atof(strtok(NULL," \n"));
//			if ((i==25)) 
//			{
//				int ttt=0;
//			}
//
//			//这个暂时注释掉 因为暂时不清楚如何进行修改，后面再来修改 2014.03.10
//			//if (temp!=9999)
//			//{
//			//	index.x= j;
//			//	index.y= i;
//			//	m_vector.push_back(index);
//			//}
//
//			if(temp!=9999){
//            double X=xllCorner+j*fCellsize;
//			double Y=yllCorner+(nHeight-i)*fCellsize; 
//			UTM2LatLon* c = new UTM2LatLon();
//			double  LatLong[2];
//
//			stringstream ss;
//
//			stringstream strX,strY;
//			strX<<X;
//			strY<<Y;			
//
//			std::string s1=strX.str();  
//			std::string s2=strY.str();  
//
//			std::string ssss="48 N "+s1+" "+s2;
//
//			c->convertUTMToLatLong(ssss,LatLong);
//
//			p2DCellArray[i][j].Lat=LatLong[0]*3.1415926535897932384626/180; //转换为弧度
//			p2DCellArray[i][j].Long=LatLong[1]*3.1415926535897932384626/180;//转换为弧度
//			}
//			else
//			{
//			p2DCellArray[i][j].Lat=0; //转换为弧度
//			p2DCellArray[i][j].Long=0;//转换为弧度
//			}
//
//			p2DCellArray[i][j].DemData= temp;
//			p2DCellArray[i][j].WaterHeight_New=0;
//			p2DCellArray[i][j].WaterHeight_Old= temp;
//			p2DCellArray[i][j].XUnitFlux_New=0;
//			p2DCellArray[i][j].XUnitFlux_Old=0;
//			p2DCellArray[i][j].YUnitFlux_New=0;
//			p2DCellArray[i][j].YUnitFlux_Old=0;
//			p2DCellArray[i][j].b_IsRiver=false;
//			p2DCellArray[i][j].b_IsOrigin=false;
//			p2DCellArray[i][j].b_IsEnd=false;
//			p2DCellArray[i][j].vec= 0;
//		}
////销毁临时内存，关闭文件
//	delete []pData;
//	fclose(file);
//}