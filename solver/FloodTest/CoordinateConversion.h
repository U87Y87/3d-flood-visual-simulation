#include <math.h>   
#include <stdio.h>
#include <string>
#include <iostream>
using namespace std;
//坐标转换头文件
#define MyPI 3.14159265;
static	char letters1[22] ={ 'A', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Z' };
static int degrees[22] = { -90, -84, -72, -64, -56, -48, -40, -32, -24, -16, -8, 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 84 };
static char negLetters[11]  = { 'A', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 'K', 'L', 'M' };
static int negDegrees[11] = { -90, -84, -72, -64, -56, -48, -40, -32, -24, -16, -8 };
static char posLetters[11] = { 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Z' };
static int posDegrees[11] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 84 };  



 class LatZones
  {
  private:
	/*  char *letters;
      int *degrees ;
      char *negLetters;
      int *negDegrees ;
      char *posLetters;
      int *posDegrees; 
	  */
	  int arrayLength;


  public:
	  LatZones(); 

   int getLatZoneDegree(string letter);   

   string getLatZone(double latitude);
};


class LatLon2UTM
  {
  public:
	  string convertLatLonToUTM(double latitude, double longitude);//输入经纬度，计算对应WGS84 坐标
      LatLon2UTM();
	  int X,Y; //获取计算后的X,Y值

private:
	void validate(double latitude, double longitude)
  {
    if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0
		|| longitude >= 180.0)
	{}
      };
	   double degreeToRadian(double degree)
  {
    return degree* 3.14159265/180;
  }


private:
	double POW(double a, double b)
  {
    return pow(a, b);
  };

private:
	double SIN(double value)
  {
    return sin(value);
  };

private:
	double COS(double value)
  {
    return cos(value);
  };

private:
	double TAN(double value)
  {
    return tan(value);
  };


  protected:
	void setVariables(double latitude, double longitude);
    string getLongZone(double longitude);
    double getNorthing(double latitude);
    double getEasting();
    // Lat Lon to UTM variables

    // equatorial radius
    double equatorialRadius;
;
    // polar radius
    double polarRadius;

    // flattening
    double flattening ;// (equatorialRadius-polarRadius)/equatorialRadius;

    // inverse flattening 1/flattening
    double inverseFlattening;// 1/flattening;

    // Mean radius
    double rm ;

    // scale factor
    double k0 ;

    // eccentricity
    double e ;

    double e1sq ;

    double n ;

    // r curv 1
    double rho ;

    // r curv 2
    double nu;

    // Calculate Meridional Arc Length
    // Meridional Arc
    double S ;

    double A0;

    double B0;

    double C0;

    double D0;

    double E0;

    // Calculation Constants
    // Delta Long
    double p ;

    double sin1;

    // Coefficients for UTM Coordinates
    double K1;

    double K2;

    double K3;

    double K4;

    double K5;

    double A6;

  };
class UTM2LatLon
  {
public:
	double easting;
	double northing;

	UTM2LatLon();

	int zone;
	string southernHemisphere;
	std::string getHemisphere(string latZone);
	void convertUTMToLatLong(std::string UTM, double latlon[2]);
	void setVariables();

	private:
	double POW(double a, double b)
  {
    return pow(a, b);
  };

	private:
	double SIN(double value)
  {
    return sin(value);
  };

private:
	double COS(double value)
  {
    return cos(value);
  };

private:
	double TAN(double value)
  {
    return tan(value);
  };


  protected:
	double arc;
    double mu;
    double ei;
    double ca;
    double cb;
    double cc;
    double cd;
    double n0;
    double r0;
    double _a1;
    double dd0;
    double t0;
    double Q0;
    double lof1;
    double lof2;
    double lof3;
    double _a2;
    double phi1;
    double fact1;
    double fact2;
    double fact3;
    double fact4;
    double zoneCM;
    double _a3;
    double b;
    double a;
    double e;
    double e1sq;
    double k0;
  };



class CoordinateConversion{

public:
	CoordinateConversion();

string latLon2UTM(double latitude, double longitude)
  {
    LatLon2UTM c;// = new LatLon2UTM;
    return c.convertLatLonToUTM(latitude, longitude);
  };


};

