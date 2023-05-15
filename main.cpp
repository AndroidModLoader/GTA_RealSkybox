#include <mod/amlmod.h>
#include <mod/logger.h>

#include "GTASA_STRUCTS.h"
#include "skybox.h"

MYMOD(net.juniordjjr.rusjj.realskybox, GTA Real Skybox, 1.0, Junior Djrr & RusJJ)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.0.2.1)
END_DEPLIST()

#define MAGIC_FLOAT       (5.0f / 3.0f)
#define WEATHER_FOR_STARS (eWeatherType::WEATHER_UNDERWATER)

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Saves     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t pGTASA;
void* hGTASA;
RpAtomic* pSkyAtomic = NULL;
RwFrame* pSkyFrame = NULL;
Skybox *aSkyboxes[eWeatherType::WEATHER_UNDERWATER + 1];
CVector vecOldSkyboxScale, vecNewSkyboxScale, vecStarsSkyboxScale, vecCloudsRotationVector, vecStarsRotationVector;
bool bChangeWeather = true, bUsingInterp = false, bProcessedFirst = false;
float fTestInterp = 0.0f, fInCityFactor = 0.0f;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Vars      ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CCamera* TheCamera;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
bool (*CalcScreenCoors)(CVector*, CVector*, float*, float*, bool, bool);
void (*RenderBufferedOneXLUSprite)(CVector pos, float w, float h, uint8_t r, uint8_t g, uint8_t b, short intens, float recipZ, uint8_t a);

/////////////////////////////////////////////////////////////////////////////
//////////////////////////////     Patches     //////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t _BackTo;
extern "C" void _Patch(float intensity)
{
    
}
__attribute__((optnone)) __attribute__((naked)) void _inject(void)
{
    asm volatile(
        "PUSH            {R0-R11}\n"
        "VMOV            R0, S20\n"
        "VPUSH           {S0-S31}\n"
        "BL              _Patch\n"
        "VPOP            {S0-S31}\n");
    asm volatile(
        "MOV             R12, %0\n"
        "POP             {R0-R11}\n"
        "BX              R12\n"
    :: "r" (_BackTo));
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
void PrepareSkyboxModel()
{
    
}
void SetInUseForThisTexture(RwTexture *tex)
{
	for (int i = 0; i <= eWeatherType::WEATHER_UNDERWATER; ++i)
	{
		if (tex == aSkyboxes[i]->tex) aSkyboxes[i]->inUse = true;
	}
}

void SetRotationForThisTexture(RwTexture *tex, float rot)
{
	for (int i = 0; i <= eWeatherType::WEATHER_UNDERWATER; ++i)
	{
		if (tex == aSkyboxes[i]->tex) aSkyboxes[i]->rot = rot;
	}
}

bool NoSunriseWeather(eWeatherType id)
{
	return (id == eWeatherType::WEATHER_CLOUDY_COUNTRYSIDE || id == eWeatherType::WEATHER_CLOUDY_LA || id == eWeatherType::WEATHER_CLOUDY_SF || id == eWeatherType::WEATHER_CLOUDY_VEGAS ||
			id == eWeatherType::WEATHER_RAINY_COUNTRYSIDE || id == eWeatherType::WEATHER_RAINY_SF || id == eWeatherType::WEATHER_FOGGY_SF);
}
extern "C" void OnModPreLoad()
{
    logger->SetTag("RealSkybox");
    
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    
    // GTA Variables
    SET_TO(TheCamera, aml->GetSym(hGTASA, "TheCamera"));

    // GTA Functions
    SET_TO(CalcScreenCoors, aml->GetSym(hGTASA, "_ZN7CSprite15CalcScreenCoorsERK5RwV3dPS0_PfS4_bb"));
    
    // GTA Patches
    //_BackTo = pGTASA + 0x + 0x1;
    //aml->Redirect(pGTASA + 0x + 0x1, (uintptr_t)_inject);
}