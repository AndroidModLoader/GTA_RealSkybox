#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>
#include <cstdio>
#include <ctime>

#ifdef AML32
    #include "GTASA_STRUCTS.h"
    #define BYVER(__for32, __for64) (__for32)
#else
    #include "GTASA_STRUCTS_210.h"
    #define BYVER(__for32, __for64) (__for64)
#endif
#define sizeofA(__aVar)  ((int)(sizeof(__aVar)/sizeof(__aVar[0])))
#include "skybox.h"

MYMOD(net.juniordjjr.rusjj.realskybox, Real Skybox, 0.5, Junior Djrr & RusJJ)
BEGIN_DEPLIST()
    ADD_DEPENDENCY_VER(net.rusjj.aml, 1.2.1)
END_DEPLIST()
Config* cfg = new Config("RealSkybox.SA");

//#define RENDER_MIRRORED
#define MAGIC_FLOAT       (float)(50.0 / 30.0)
#define WEATHER_FOR_STARS (eWeatherType::WEATHER_UNDERWATER)

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Saves     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uintptr_t pGTASA;
void* hGTASA;
RpAtomic* pSkyAtomic = NULL;
RwFrame* pSkyFrame = NULL;
Skybox *aSkyboxes[eWeatherType::WEATHER_UNDERWATER + 1];
CVector oldSkyboxScale, newSkyboxScale, starsSkyboxScale, cloudsRotationVector, starsRotationVector;
bool changeWeather = true, usingInterp = false, processedFirst = false, skyScaleAsOnPC = false;
float testInterp = 0.0f, inCityFactor = 0.0f;
CVector ZAxis(0.0f, 0.0f, 1.0f);
CVector XYAxis(1.0f, 1.0f, 0.0f);
RwRGBAReal vecSkyColor = {1.0f, 1.0f, 1.0f, 1.0f};
float increaseRot = 0.0f, windStrengthRot = 0.0f, windRotScale = 0.006f;
bool sunReflectionChanged = false, skyboxDrawAfter = true, windStrengthAffectsRotation = true;
float lastFarClip = 0.0f, minFarPlane = 1100.0f, gameDefaultFogDensity = 1.0f, fogDensityDefault = 0.0012f, fogDensity = fogDensityDefault;
int skyboxFogType = 2;
float cloudsRotationSpeed = 0.002f, starsRotationSpeed = 0.0002f, skyboxSizeXY = 0.4f, skyboxSizeZ = 0.2f, cloudsMultBrightness = 0.4f,
      cloudsNightDarkLimit = 0.8f, cloudsMinBrightness = 0.05f, cloudsCityOrange = 1.0f, starsCityAlphaRemove = 0.8f, cloudsMultSunrise = 2.5f;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Vars      ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
CCamera *TheCamera;
RpAtomicCallBackRender AtomicDefaultRenderCallBack;
float *ms_fTimeScale, *ms_fTimeStep, *UnderWaterness, *InterpolationValue, *Wind, *currentFarClip;
uint32_t *TempBufferIndicesStored, *TempBufferVerticesStored;
uint16_t *NewWeatherType, *OldWeatherType, *ForcedWeatherType;
uint8_t *ms_nGameClockMonths, *ms_nGameClockHours;
int *m_bExtraColourOn, *m_CurrentStoredValue;
eWeatherRegion *WeatherRegion;
CColourSet *m_CurrentColours;
CVector *m_VectorToSun;
bool *m_aCheatsActive;

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
RwStream*       (*RwStreamOpen)(int, int, const char*);
bool            (*RwStreamFindChunk)(RwStream*, int, int, int);
RpClump*        (*RpClumpStreamRead)(RwStream*);
void            (*RwStreamClose)(RwStream*, int);
RpAtomic*       (*GetFirstAtomic)(RpClump*);
void            (*SetFilterModeOnAtomicsTextures)(RpAtomic*, int);
void            (*RpGeometryLock)(RpGeometry*, int);
void            (*RpGeometryUnlock)(RpGeometry*);
void            (*RpGeometryForAllMaterials)(RpGeometry*, RpMaterial* (*)(RpMaterial*, RwRGBA&), RwRGBA&);
void            (*RpMaterialSetTexture)(RpMaterial*, RwTexture*);
RpAtomic*       (*RpAtomicClone)(RpAtomic*);
void            (*RpClumpDestroy)(RpClump*);
RwFrame*        (*RwFrameCreate)();
void            (*RpAtomicSetFrame)(RpAtomic*, RwFrame*);
void            (*RenderAtomicWithAlpha)(RpAtomic*, int alphaVal);
RpGeometry*     (*RpGeometryCreate)(int, int, unsigned int);
RpMaterial*     (*RpGeometryTriangleGetMaterial)(RpGeometry*, RpTriangle*);
void            (*RpGeometryTriangleSetMaterial)(RpGeometry*, RpTriangle*, RpMaterial*);
void            (*RpAtomicSetGeometry)(RpAtomic*, RpGeometry*, unsigned int);
void            (*RwFrameUpdateObjects)(RwFrame*);
float           (*GetDayNightBalance)();
RwImage*        (*RtPNGImageRead)(const char* filename);
void            (*RwImageFindRasterFormat)(RwImage*, int, int*, int*, int*, int*);
RwRaster*       (*RwRasterCreate)(int, int, int, int);
void            (*RwRasterSetFromImage)(RwRaster*, RwImage*);
void            (*RwImageDestroy)(RwImage*);
RwTexture*      (*RwTextureCreate)(RwRaster*);
void            (*RwTextureDestroy)(RwTexture*);
void            (*RwFrameTranslate)(RwFrame*, CVector*, RwOpCombineType);
void            (*RwFrameScale)(RwFrame*, CVector*, RwOpCombineType);
void            (*RwFrameRotate)(RwFrame*, CVector*, float, RwOpCombineType);
TextureDatabaseRuntime* (*TextureDatabaseLoad)(const char*, bool, TextureDatabaseFormat);
void            (*TextureDatabaseRegister)(TextureDatabase*);
void            (*TextureDatabaseUnregister)(TextureDatabase*);
RwTexture*      (*TextureDatabaseGetTexture)(const char*);
void            (*DeActivateDirectional)();
void            (*SetAmbientColours)(RwRGBAReal*);
void            (*SetFullAmbient)();
void            (*RwRenderStateSet)(RwRenderState, void *);

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Funcs     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
/*RwTexture* GetTexIfLoaded(const char* name)
{
    for (int i = 0; i <= eWeatherType::WEATHER_UNDERWATER; ++i)
    {
        if(aSkyboxes[i]->tex && !strncmp(aSkyboxes[i]->tex->name, name, rwTEXTUREBASENAMELENGTH)) return aSkyboxes[i]->tex;
    }
    return NULL;
}*/
inline void LoadSkyboxTextures()
{
    FILE *file;
    char line[256], textureName[rwTEXTUREBASENAMELENGTH];
    bool bStartLoading = false;
    int weatherId = 0;

    // Loading ONLY DXT because it's unpixelated and looks way more better!
    TextureDatabase* tdb = TextureDatabaseLoad("realskybox", false, DF_DXT);
    if(!tdb) return;

    TextureDatabaseRegister(tdb);

    snprintf(line, sizeof(line), "%s/texdb/realskybox/skyboxes.dat", aml->GetAndroidDataPath());
    if ((file = fopen(line, "r")) == NULL) return;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        if(line[0] == 0 || line[0] == '#' || line[0] == ';' || line[0] == '/' ||
          (line[0] == '\t' && line[1] == 0) || (line[0] == ' ' && line[1] == 0)) continue;
        if(!bStartLoading)
        {
            if(!strncmp(line, "skytexs", 7)) bStartLoading = true;
            continue;
        }

        if(!strncmp(line, "end", 3)) break;

        if((sscanf(line, "%d, %31s", &weatherId, textureName) == 2 || 
            sscanf(line, "%d %31s", &weatherId, textureName) == 2) && weatherId <= WEATHER_FOR_STARS)
        {
            textureName[rwTEXTUREBASENAMELENGTH - 1] = 0; // overflow fix
            //aSkyboxes[weatherId]->tex = GetTexIfLoaded(textureName);
            //if (!aSkyboxes[weatherId]->tex)
            {
                aSkyboxes[weatherId]->tex = TextureDatabaseGetTexture(textureName);
                if (aSkyboxes[weatherId]->tex)
                {
                    aSkyboxes[weatherId]->tex->filterAddressing = rwFILTERLINEARMIPLINEAR;
                }
            }
        }
    }

    TextureDatabaseUnregister(tdb);
    fclose(file);
}
inline void PrepareSkyboxModel()
{
    RwStream *stream = RwStreamOpen(2, 1, "texdb/realskybox/skybox.dff");
    if (stream)
    {
        if (RwStreamFindChunk(stream, 16, 0, 0))
        {
            RpClump *clump = RpClumpStreamRead(stream);
            if (clump)
            {
                RpAtomic *atomic = GetFirstAtomic(clump);
                atomic->renderCallBack = AtomicDefaultRenderCallBack;

                if (atomic)
                {
                    atomic->geometry->flags |= rpGEOMETRYMODULATEMATERIALCOLOR;
                    pSkyAtomic = atomic;
                    pSkyFrame = RwFrameCreate();
                    RpAtomicSetFrame(pSkyAtomic, pSkyFrame);
                    RwFrameUpdateObjects(pSkyFrame);
                }
            }
        }
    }
    RwStreamClose(stream, 0);
}
inline void SetInUseForThisTexture(RwTexture *tex)
{
    for (int i = 0; i <= eWeatherType::WEATHER_UNDERWATER; ++i)
    {
        if (tex == aSkyboxes[i]->tex) aSkyboxes[i]->inUse = true;
    }
}
inline void SetRotationForThisTexture(RwTexture *tex, float rot)
{
    for (int i = 0; i <= eWeatherType::WEATHER_UNDERWATER; ++i)
    {
        if (tex == aSkyboxes[i]->tex) aSkyboxes[i]->rot = rot;
    }
}
inline bool NoSunriseWeather(eWeatherType id)
{
    return (id == eWeatherType::WEATHER_CLOUDY_COUNTRYSIDE || id == eWeatherType::WEATHER_CLOUDY_LA || id == eWeatherType::WEATHER_CLOUDY_SF || id == eWeatherType::WEATHER_CLOUDY_VEGAS ||
            id == eWeatherType::WEATHER_RAINY_COUNTRYSIDE || id == eWeatherType::WEATHER_RAINY_SF || id == eWeatherType::WEATHER_FOGGY_SF);
}
void RenderSkybox()
{
    int oldWeatherType = *OldWeatherType;
    int newWeatherType = *NewWeatherType;
    if (oldWeatherType > eWeatherType::WEATHER_UNDERWATER) oldWeatherType = eWeatherType::WEATHER_SUNNY_LA;
    if (newWeatherType > eWeatherType::WEATHER_UNDERWATER) newWeatherType = eWeatherType::WEATHER_SUNNY_LA;

    if (increaseRot > 0.0f)
    {
        increaseRot -= 0.0064f * *ms_fTimeStep * MAGIC_FLOAT;
        if (increaseRot < 0.0f) increaseRot = 0.0f;
    }

    // Tweak by distance
    float farPlane = TheCamera->m_pRwCamera->farClip;
    //float scaleFactor = 0.95f * farPlane / pSkyAtomic->boundingSphere.radius / 0.4f; // old
    float scaleFactor = 1.1f * farPlane / pSkyAtomic->boundingSphere.radius;
    float goodDistanceFactor = (farPlane - 1000.0f) / 1000.0f; //  if farPlane is 2000.0, goodDistanceFactor is 2.0
    if (goodDistanceFactor < 0.01f) goodDistanceFactor = 0.01f;

    if (skyboxFogType <= 1) //linear
    {
        fogDensity = gameDefaultFogDensity;
    }
    else
    {
        fogDensity = fogDensityDefault / goodDistanceFactor;
        if (*UnderWaterness > 0.4f)
        {
            fogDensity *= 1.0f + ((*UnderWaterness - 0.4f) * 100.0f);
            fogDensity *= 0.1f;
        }
    }

    oldSkyboxScale.x = skyboxSizeXY;
    oldSkyboxScale.y = skyboxSizeXY;
    oldSkyboxScale.z = skyboxSizeZ;
    
    // new in 0.4
    oldSkyboxScale *= skyScaleAsOnPC ? goodDistanceFactor : scaleFactor;

    newSkyboxScale.x = oldSkyboxScale.x * 1.05f;
    newSkyboxScale.y = oldSkyboxScale.y * 1.05f;
    newSkyboxScale.z = oldSkyboxScale.z * 1.05f;

    starsSkyboxScale.x = newSkyboxScale.x * 1.05f;
    starsSkyboxScale.y = newSkyboxScale.y * 1.05f;
    starsSkyboxScale.z = newSkyboxScale.z * 1.05f;

    float oldAlpha = (1.0f - *InterpolationValue) * 255.0f;
    float newAlpha = *InterpolationValue * 255.0f;
    float dayNightBalance = GetDayNightBalance();

    // Get position
    CVector camPos = TheCamera->GetPosition();

    for (int i = 0; i <= eWeatherType::WEATHER_UNDERWATER; ++i)
    {
        if (!aSkyboxes[i]->inUse)
        {
            if (i == WEATHER_FOR_STARS)
            {
                aSkyboxes[i]->rot = *ms_nGameClockMonths * 30.0f; // stars always starts with rotation based on month
            }
            else
            {
                srand(time(NULL));
                aSkyboxes[i]->rot = rand() * (360.0 / RAND_MAX);
            }
        }
        aSkyboxes[i]->inUse = false; // reset flag
    }

    if (*WeatherRegion == eWeatherRegion::WEATHER_REGION_DEFAULT || *WeatherRegion == eWeatherRegion::WEATHER_REGION_DESERT)
    {
        inCityFactor -= 0.001f * *ms_fTimeStep * MAGIC_FLOAT;
        if (inCityFactor < 0.0f) inCityFactor = 0.0f;
    }
    else
    {
        inCityFactor += 0.001f * *ms_fTimeStep * MAGIC_FLOAT;
        if (inCityFactor > 1.0f) inCityFactor = 1.0f;
    }

    // Get stars alpha
    float starsAlpha = 0.0f;
    if (dayNightBalance > 0.0f)
    {
        float skyIllumination = (m_CurrentColours->skybotr + m_CurrentColours->skybotg + m_CurrentColours->skybotb + m_CurrentColours->skytopr + m_CurrentColours->skytopg + m_CurrentColours->skytopb) / 255.0f;
        if (skyIllumination > 1.0f) skyIllumination = 1.0f;
        starsAlpha = (1.0f - skyIllumination) * dayNightBalance;
        starsAlpha -= 1.0f * (inCityFactor * (starsCityAlphaRemove / 2.0f));
    }

    // Next weather texture is different from current?
    bool newTexIsDifferent = (aSkyboxes[oldWeatherType]->tex != aSkyboxes[newWeatherType]->tex);
    if (!newTexIsDifferent)
    {
        oldAlpha += newAlpha;
        if (oldAlpha > 255.0f) oldAlpha = 255.0f;
    }

    // Process rotation
    if (m_aCheatsActive[0x13]) // fast clock
    {
        aSkyboxes[oldWeatherType]->rot += 0.1f + increaseRot * *ms_fTimeScale * (*ms_fTimeStep * MAGIC_FLOAT);
        if (newTexIsDifferent) aSkyboxes[newWeatherType]->rot += (0.1f * 0.7f) + increaseRot * *ms_fTimeScale * (*ms_fTimeStep * MAGIC_FLOAT);
        aSkyboxes[WEATHER_FOR_STARS]->rot += 0.005f + increaseRot * *ms_fTimeScale * (*ms_fTimeStep * MAGIC_FLOAT);
    }
    else
    {
        if(windStrengthAffectsRotation) windStrengthRot = *Wind * windRotScale;
        else windStrengthRot = 0;

        aSkyboxes[oldWeatherType]->rot += (cloudsRotationSpeed * 0.5f) + (increaseRot + windStrengthRot) * *ms_fTimeScale * (*ms_fTimeStep * MAGIC_FLOAT);
        if (newTexIsDifferent) aSkyboxes[newWeatherType]->rot += (cloudsRotationSpeed * 0.5f * 0.7f) + (increaseRot + windStrengthRot) * *ms_fTimeScale * (*ms_fTimeStep * MAGIC_FLOAT);
        aSkyboxes[WEATHER_FOR_STARS]->rot += (starsRotationSpeed * 0.5f) + increaseRot * *ms_fTimeScale * (*ms_fTimeStep * MAGIC_FLOAT);
    }
    while (aSkyboxes[oldWeatherType]->rot > 360.0f) aSkyboxes[oldWeatherType]->rot -= 360.0f;
    while (aSkyboxes[newWeatherType]->rot > 360.0f) aSkyboxes[newWeatherType]->rot -= 360.0f;
    while (aSkyboxes[WEATHER_FOR_STARS]->rot > 360.0f) aSkyboxes[WEATHER_FOR_STARS]->rot -= 360.0f;
    SetRotationForThisTexture(aSkyboxes[oldWeatherType]->tex, aSkyboxes[oldWeatherType]->rot);
    if (newTexIsDifferent) SetRotationForThisTexture(aSkyboxes[newWeatherType]->tex, aSkyboxes[newWeatherType]->rot);

    // Get Ilumination
    float skyboxIllumination = ((m_CurrentColours->skybotr + m_CurrentColours->skybotg + m_CurrentColours->skybotb) * cloudsMultBrightness) / 255.0f;
    if (skyboxIllumination > 1.0f) skyboxIllumination = 1.0f;
    if (dayNightBalance != 0.0f && inCityFactor != 0.0f) skyboxIllumination -= (dayNightBalance / 12.0f) * (1.0f - inCityFactor);
    float dayNightBalanceReverse = (1.0f - dayNightBalance);
    if (dayNightBalanceReverse < cloudsNightDarkLimit) dayNightBalanceReverse = cloudsNightDarkLimit;
    skyboxIllumination *= dayNightBalanceReverse;
    if (skyboxIllumination < cloudsMinBrightness) skyboxIllumination = cloudsMinBrightness;
    if (skyboxIllumination > 1.0f) skyboxIllumination = 1.0f;

    // Get color
    float sunriseFactor = 0.0f;
    float sunHorizonFactor = m_VectorToSun[*m_CurrentStoredValue].z;
    if (sunHorizonFactor > 0.0f)
    {
        if (sunHorizonFactor > 0.2f) sunHorizonFactor -= (sunHorizonFactor - 0.2f) * 2.0f; // 0.0 - 0.2 - 0.0
        sunriseFactor = sunHorizonFactor * 10.0f;
        if (sunriseFactor > 1.0f) sunriseFactor = 1.0f;
        if (NoSunriseWeather((eWeatherType)oldWeatherType)) sunriseFactor -= (oldAlpha / 255.0f);
        if (NoSunriseWeather((eWeatherType)newWeatherType)) sunriseFactor -= (newAlpha / 255.0f);
        if (sunriseFactor > 0.0f)
        {
            sunriseFactor *= cloudsMultSunrise;
            if (sunHorizonFactor > 0.0f) sunriseFactor += (abs(1.0f - sunHorizonFactor) * sunHorizonFactor);
        }
        else
        {
            sunriseFactor = 0.0f;
        }
    }
    sunriseFactor += ((cloudsCityOrange / 4.0f) * inCityFactor) * dayNightBalance;

    vecSkyColor =
    {
        skyboxIllumination,
        (skyboxIllumination - (sunriseFactor / 16.0f)),
        (skyboxIllumination - (sunriseFactor / 10.0f)),
        1.0f,
    };

    RwRenderStateSet(rwRENDERSTATEVERTEXALPHAENABLE, (void*)true);
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)true);
    RwRenderStateSet(rwRENDERSTATEFOGTYPE, (void*)(intptr_t)skyboxFogType); // warning: cast to 'void *' from smaller integer type 'int'
    RwRenderStateSet(rwRENDERSTATEFOGDENSITY, &fogDensity);

    // Fixing a glitch
    RwRenderStateSet(rwRENDERSTATEZWRITEENABLE, (void*)false);
    RwRenderStateSet(rwRENDERSTATEZTESTENABLE, (void*)false);
    RwRenderStateSet(rwRENDERSTATESRCBLEND, (void*)rwBLENDSRCALPHA);
    RwRenderStateSet(rwRENDERSTATEDESTBLEND, (void*)rwBLENDINVSRCALPHA);

    // Render skyboxes
    if (starsAlpha > 0.0f) // Stars
    {
        RwFrameTranslate(pSkyFrame, &camPos, rwCOMBINEREPLACE);
        RwFrameScale(pSkyFrame, &starsSkyboxScale, rwCOMBINEPRECONCAT);
        RwFrameRotate(pSkyFrame, &ZAxis, aSkyboxes[WEATHER_FOR_STARS]->rot, rwCOMBINEPRECONCAT);
        RwFrameUpdateObjects(pSkyFrame);

        pSkyAtomic->geometry->matList.materials[0]->texture = aSkyboxes[WEATHER_FOR_STARS]->tex;
        aSkyboxes[WEATHER_FOR_STARS]->inUse = true;

        int finalAlpha = (int)(starsAlpha * 255.0f);
        if (skyboxFogType <= 1 && *UnderWaterness > 0.4f) finalAlpha /= 1.0f + ((*UnderWaterness - 0.4f) * 100.0f);

        #ifdef RENDER_MIRRORED
            CVector mirrorCamPos = camPos;
            mirrorCamPos.z -= 2.0f * scaleFactor * pSkyAtomic->boundingSphere.center.z;
            RwFrameTranslate(pSkyFrame, &mirrorCamPos, rwCOMBINEREPLACE);
            RwFrameRotate(pSkyFrame, &XYAxis, 180.0f, rwCOMBINEPRECONCAT);
            RwFrameScale(pSkyFrame, &starsSkyboxScale, rwCOMBINEPRECONCAT);
            RwFrameUpdateObjects(pSkyFrame);
            RenderAtomicWithAlpha(pSkyAtomic, finalAlpha);
        #endif

        SetFullAmbient();
        DeActivateDirectional();
        pSkyAtomic->geometry->matList.materials[0]->color.alpha = finalAlpha;
        RenderAtomicWithAlpha(pSkyAtomic, finalAlpha);
    }

    if (aSkyboxes[oldWeatherType]->tex && oldAlpha > 0.0f) // Old (current)
    {
        RwFrameTranslate(pSkyFrame, &camPos, rwCOMBINEREPLACE);
        RwFrameScale(pSkyFrame, &oldSkyboxScale, rwCOMBINEPRECONCAT);
        RwFrameRotate(pSkyFrame, &ZAxis, aSkyboxes[oldWeatherType]->rot, rwCOMBINEPRECONCAT);
        RwFrameUpdateObjects(pSkyFrame);

        pSkyAtomic->geometry->matList.materials[0]->texture = aSkyboxes[oldWeatherType]->tex;
        SetInUseForThisTexture(aSkyboxes[oldWeatherType]->tex);

        int finalAlpha = (int)oldAlpha;
        if (skyboxFogType <= 1 && *UnderWaterness > 0.4f) finalAlpha /= 1.0f + ((*UnderWaterness - 0.4f) * 100.0f);

        #ifdef RENDER_MIRRORED
            CVector mirrorCamPos = camPos;
            mirrorCamPos.z -= 2.0f * scaleFactor * pSkyAtomic->boundingSphere.center.z;
            RwFrameTranslate(pSkyFrame, &mirrorCamPos, rwCOMBINEREPLACE);
            RwFrameRotate(pSkyFrame, &XYAxis, 180.0f, rwCOMBINEPRECONCAT);
            RwFrameScale(pSkyFrame, &oldSkyboxScale, rwCOMBINEPRECONCAT);
            RwFrameUpdateObjects(pSkyFrame);
            RenderAtomicWithAlpha(pSkyAtomic, finalAlpha);
        #endif

        SetAmbientColours(&vecSkyColor);
        DeActivateDirectional();
        pSkyAtomic->geometry->matList.materials[0]->color.alpha = finalAlpha;
        RenderAtomicWithAlpha(pSkyAtomic, finalAlpha);
    }

    if (newTexIsDifferent && aSkyboxes[newWeatherType]->tex && newAlpha > 0.0f) // New (next)
    {
        RwFrameTranslate(pSkyFrame, &camPos, rwCOMBINEREPLACE);
        RwFrameScale(pSkyFrame, &newSkyboxScale, rwCOMBINEPRECONCAT);
        RwFrameRotate(pSkyFrame, &ZAxis, aSkyboxes[newWeatherType]->rot, rwCOMBINEPRECONCAT);
        RwFrameUpdateObjects(pSkyFrame);

        pSkyAtomic->geometry->matList.materials[0]->texture = aSkyboxes[newWeatherType]->tex;
        SetInUseForThisTexture(aSkyboxes[newWeatherType]->tex);

        int finalAlpha = (int)newAlpha;
        if (skyboxFogType <= 1 && *UnderWaterness > 0.4f) finalAlpha /= 1.0f + ((*UnderWaterness - 0.4f) * 100.0f);

        #ifdef RENDER_MIRRORED
            CVector mirrorCamPos = camPos;
            mirrorCamPos.z -= 2.0f * scaleFactor * pSkyAtomic->boundingSphere.center.z;
            RwFrameTranslate(pSkyFrame, &mirrorCamPos, rwCOMBINEREPLACE);
            RwFrameRotate(pSkyFrame, &XYAxis, 180.0f, rwCOMBINEPRECONCAT);
            RwFrameScale(pSkyFrame, &newSkyboxScale, rwCOMBINEPRECONCAT);
            RwFrameUpdateObjects(pSkyFrame);
            RenderAtomicWithAlpha(pSkyAtomic, finalAlpha);
        #endif

        SetAmbientColours(&vecSkyColor);
        DeActivateDirectional();
        pSkyAtomic->geometry->matList.materials[0]->color.alpha = finalAlpha;
        RenderAtomicWithAlpha(pSkyAtomic, finalAlpha);
    }

    RwRenderStateSet(rwRENDERSTATEFOGDENSITY, &gameDefaultFogDensity);
    RwRenderStateSet(rwRENDERSTATEFOGTYPE, (void*)1);
    RwRenderStateSet(rwRENDERSTATEFOGENABLE, (void*)false);
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Hooks     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
uint32_t sunReflectionsCount = 20;
#ifdef AML32
uint32_t minFarClipPatchValue = 1440; // default
#else
uint32_t minFarClipPatchValue = 45; // default
#endif
DECL_HOOKb(GameInit3, void* data)
{
    GameInit3(data);

    for (int i = 0; i <= eWeatherType::WEATHER_UNDERWATER; ++i) { aSkyboxes[i] = new Skybox(); }

    LoadSkyboxTextures();
    PrepareSkyboxModel();

    return true;
}
DECL_HOOKv(RenderClouds)
{
    if(!skyboxDrawAfter) RenderSkybox();
    RenderClouds();
    if( skyboxDrawAfter) RenderSkybox();
}
DECL_HOOKv(TimecycUpdate)
{
    TimecycUpdate();

    if (*m_bExtraColourOn == 0 && *OldWeatherType != eWeatherType::WEATHER_UNDERWATER && *NewWeatherType != eWeatherType::WEATHER_UNDERWATER && *ForcedWeatherType != eWeatherType::WEATHER_UNDERWATER && *UnderWaterness < 0.4f)
    {
        if (*currentFarClip < minFarPlane)
        {
            *currentFarClip = minFarPlane;
        }

        // sunReflectionChanged is IF IT WAS PATCHED ALREADY (some limit adjuster?) <- false for us, always
        if (lastFarClip != *currentFarClip && !sunReflectionChanged)
        {
            // default is 1440 for 20:
            sunReflectionsCount = 20 * (*currentFarClip / 1000.0f);

          #ifdef AML32
            minFarClipPatchValue = 72 * sunReflectionsCount;
          #else
            minFarClipPatchValue = 4 + sunReflectionsCount * 2 + 1;
          #endif
        }
        lastFarClip = *currentFarClip;
    }
}
DECL_HOOKv(SetRenderState_SunRefl, void* state, void* value)
{
    SetRenderState_SunRefl(state, value);
    
    *TempBufferVerticesStored = 4 + sunReflectionsCount * 2;
    *TempBufferIndicesStored = 6 + sunReflectionsCount * 6;
}
DECL_HOOKv(GameLogicPassTime, unsigned int time)
{
    if (TheCamera->m_fFloatingFade > 160.0f)
    {
        int oldWeatherType = *OldWeatherType;
        int newWeatherType = *NewWeatherType;
        if (oldWeatherType > eWeatherType::WEATHER_UNDERWATER) oldWeatherType = eWeatherType::WEATHER_SUNNY_LA;
        if (newWeatherType > eWeatherType::WEATHER_UNDERWATER) newWeatherType = eWeatherType::WEATHER_SUNNY_LA;

        float fTime = log10((float)time) * 100.0f;
        aSkyboxes[oldWeatherType]->rot += (0.1f + fTime) * (*ms_fTimeStep * MAGIC_FLOAT);
        aSkyboxes[newWeatherType]->rot += ((0.1f * 0.7f) + fTime) * (*ms_fTimeStep * MAGIC_FLOAT);
        aSkyboxes[WEATHER_FOR_STARS]->rot += (0.005f + fTime) * (*ms_fTimeStep * MAGIC_FLOAT);
    }
    else
    {
        if (*m_bExtraColourOn == 0 && *OldWeatherType != eWeatherType::WEATHER_UNDERWATER && *NewWeatherType != eWeatherType::WEATHER_UNDERWATER && *ForcedWeatherType != eWeatherType::WEATHER_UNDERWATER)
        {
            float fTime = log10((float)time) * (*ms_fTimeStep * MAGIC_FLOAT);

            float increaseLimitMin = 0.1f * (*ms_fTimeStep * MAGIC_FLOAT);
            if (fTime < increaseLimitMin) fTime = increaseLimitMin;

            increaseRot += fTime;

            float increaseLimitMax = fTime * 0.5f * (*ms_fTimeStep * MAGIC_FLOAT);
            if (increaseRot > increaseLimitMax) increaseRot = increaseLimitMax;
        }
    }
    GameLogicPassTime(time);
}

uintptr_t MinFarClip_Continue, MinFarClip_Break;
#ifdef AML32
extern "C" uintptr_t MinFarClip_Inject(int val)
{
    return (val < minFarClipPatchValue) ? MinFarClip_Continue : MinFarClip_Break;
}
__attribute__((optnone)) __attribute__((naked)) void MinFarClip_Patch(void)
{
    asm("VSTR S2, [R0, #0xB8]");
    asm("VSTR S20, [R0, #0xBC]");
    asm("PUSH {R0-R3}");
    asm("MOV R0, R10");
    asm("BL MinFarClip_Inject");
    asm("MOV R12, R0");
    asm("POP {R0-R3}");
    asm("BX R12");
}
#else
extern "C" uintptr_t MinFarClip_Inject(int val)
{
    return (val < minFarClipPatchValue) ? MinFarClip_Continue : MinFarClip_Break;
}
__attribute__((optnone)) __attribute__((naked)) void MinFarClip_Patch(void)
{
    asm("STR X28, [X9, #0xD0]");
    asm("STP S4, S5, [X9, #0x90]");
    asm("STP S1, S0, [X9, #0xB4]");
    asm("MOV W0, W26");
    asm("BL MinFarClip_Inject");
    asm("BR X0");
}
#endif
extern "C" void Stub(...) {}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Main     ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
extern "C" void OnModPreLoad()
{
    logger->SetTag("RealSkybox");
    
    pGTASA = aml->GetLib("libGTASA.so");
    hGTASA = aml->GetLibHandle("libGTASA.so");
    
    // GTA Variables
    SET_TO(TheCamera,                       aml->GetSym(hGTASA, "TheCamera"));
    SET_TO(AtomicDefaultRenderCallBack,     aml->GetSym(hGTASA, "_Z27AtomicDefaultRenderCallBackP8RpAtomic"));
    SET_TO(ms_fTimeScale,                   aml->GetSym(hGTASA, "_ZN6CTimer13ms_fTimeScaleE"));
    SET_TO(ms_fTimeStep,                    aml->GetSym(hGTASA, "_ZN6CTimer12ms_fTimeStepE"));
    SET_TO(UnderWaterness,                  aml->GetSym(hGTASA, "_ZN8CWeather14UnderWaternessE"));
    SET_TO(InterpolationValue,              aml->GetSym(hGTASA, "_ZN8CWeather18InterpolationValueE"));
    SET_TO(Wind,                            aml->GetSym(hGTASA, "_ZN8CWeather4WindE"));
    SET_TO(currentFarClip,                  pGTASA + BYVER(0x96659C, 0xBD7618));
    SET_TO(TempBufferVerticesStored,        aml->GetSym(hGTASA, "TempBufferVerticesStored"));
    SET_TO(TempBufferIndicesStored,         aml->GetSym(hGTASA, "TempBufferIndicesStored"));
    SET_TO(NewWeatherType,                  aml->GetSym(hGTASA, "_ZN8CWeather14NewWeatherTypeE"));
    SET_TO(OldWeatherType,                  aml->GetSym(hGTASA, "_ZN8CWeather14OldWeatherTypeE"));
    SET_TO(ForcedWeatherType,               aml->GetSym(hGTASA, "_ZN8CWeather17ForcedWeatherTypeE"));
    SET_TO(ms_nGameClockMonths,             aml->GetSym(hGTASA, "_ZN6CClock19ms_nGameClockMonthsE"));
    SET_TO(ms_nGameClockHours,              aml->GetSym(hGTASA, "_ZN6CClock18ms_nGameClockHoursE"));
    SET_TO(m_bExtraColourOn,                aml->GetSym(hGTASA, "_ZN10CTimeCycle16m_bExtraColourOnE"));
    SET_TO(m_CurrentStoredValue,            aml->GetSym(hGTASA, "_ZN10CTimeCycle20m_CurrentStoredValueE"));
    SET_TO(WeatherRegion,                   aml->GetSym(hGTASA, "_ZN8CWeather13WeatherRegionE"));
    SET_TO(m_CurrentColours,                aml->GetSym(hGTASA, "_ZN10CTimeCycle16m_CurrentColoursE"));
    SET_TO(m_VectorToSun,                   aml->GetSym(hGTASA, "_ZN10CTimeCycle13m_VectorToSunE"));
    SET_TO(m_aCheatsActive,                 aml->GetSym(hGTASA, "_ZN6CCheat15m_aCheatsActiveE"));

    // GTA Functions
    SET_TO(RwStreamOpen,                    aml->GetSym(hGTASA, "_Z12RwStreamOpen12RwStreamType18RwStreamAccessTypePKv"));
    SET_TO(RwStreamFindChunk,               aml->GetSym(hGTASA, "_Z17RwStreamFindChunkP8RwStreamjPjS1_"));
    SET_TO(RpClumpStreamRead,               aml->GetSym(hGTASA, "_Z17RpClumpStreamReadP8RwStream"));
    SET_TO(RwStreamClose,                   aml->GetSym(hGTASA, "_Z13RwStreamCloseP8RwStreamPv"));
    SET_TO(GetFirstAtomic,                  aml->GetSym(hGTASA, "_Z14GetFirstAtomicP7RpClump"));
    SET_TO(SetFilterModeOnAtomicsTextures,  aml->GetSym(hGTASA, "_Z30SetFilterModeOnAtomicsTexturesP8RpAtomic19RwTextureFilterMode"));
    SET_TO(RpGeometryLock,                  aml->GetSym(hGTASA, "_Z14RpGeometryLockP10RpGeometryi"));
    SET_TO(RpGeometryUnlock,                aml->GetSym(hGTASA, "_Z16RpGeometryUnlockP10RpGeometry"));
    SET_TO(RpGeometryForAllMaterials,       aml->GetSym(hGTASA, "_Z25RpGeometryForAllMaterialsP10RpGeometryPFP10RpMaterialS2_PvES3_"));
    SET_TO(RpMaterialSetTexture,            aml->GetSym(hGTASA, "_Z20RpMaterialSetTextureP10RpMaterialP9RwTexture"));
    SET_TO(RpAtomicClone,                   aml->GetSym(hGTASA, "_Z13RpAtomicCloneP8RpAtomic"));
    SET_TO(RpClumpDestroy,                  aml->GetSym(hGTASA, "_Z14RpClumpDestroyP7RpClump"));
    SET_TO(RwFrameCreate,                   aml->GetSym(hGTASA, "_Z13RwFrameCreatev"));
    SET_TO(RpAtomicSetFrame,                aml->GetSym(hGTASA, "_Z16RpAtomicSetFrameP8RpAtomicP7RwFrame"));
    SET_TO(RenderAtomicWithAlpha,           aml->GetSym(hGTASA, "_ZN18CVisibilityPlugins21RenderAtomicWithAlphaEP8RpAtomici"));
    SET_TO(RpGeometryCreate,                aml->GetSym(hGTASA, "_Z16RpGeometryCreateiij"));
    SET_TO(RpGeometryTriangleGetMaterial,   aml->GetSym(hGTASA, "_Z29RpGeometryTriangleGetMaterialPK10RpGeometryPK10RpTriangle"));
    SET_TO(RpGeometryTriangleSetMaterial,   aml->GetSym(hGTASA, "_Z29RpGeometryTriangleSetMaterialP10RpGeometryP10RpTriangleP10RpMaterial"));
    SET_TO(RpAtomicSetGeometry,             aml->GetSym(hGTASA, "_Z19RpAtomicSetGeometryP8RpAtomicP10RpGeometryj"));
    SET_TO(RwFrameUpdateObjects,            aml->GetSym(hGTASA, "_Z20RwFrameUpdateObjectsP7RwFrame"));
    SET_TO(GetDayNightBalance,              aml->GetSym(hGTASA, "_Z18GetDayNightBalancev"));
    SET_TO(RtPNGImageRead,                  aml->GetSym(hGTASA, "_Z14RtPNGImageReadPKc"));
    SET_TO(RwImageFindRasterFormat,         aml->GetSym(hGTASA, "_Z23RwImageFindRasterFormatP7RwImageiPiS1_S1_S1_"));
    SET_TO(RwRasterCreate,                  aml->GetSym(hGTASA, "_Z14RwRasterCreateiiii"));
    SET_TO(RwRasterSetFromImage,            aml->GetSym(hGTASA, "_Z20RwRasterSetFromImageP8RwRasterP7RwImage"));
    SET_TO(RwImageDestroy,                  aml->GetSym(hGTASA, "_Z14RwImageDestroyP7RwImage"));
    SET_TO(RwTextureCreate,                 aml->GetSym(hGTASA, "_Z15RwTextureCreateP8RwRaster"));
    SET_TO(RwTextureDestroy,                aml->GetSym(hGTASA, "_Z16RwTextureDestroyP9RwTexture"));
    SET_TO(RwFrameTranslate,                aml->GetSym(hGTASA, "_Z16RwFrameTranslateP7RwFramePK5RwV3d15RwOpCombineType"));
    SET_TO(RwFrameScale,                    aml->GetSym(hGTASA, "_Z12RwFrameScaleP7RwFramePK5RwV3d15RwOpCombineType"));
    SET_TO(RwFrameRotate,                   aml->GetSym(hGTASA, "_Z13RwFrameRotateP7RwFramePK5RwV3df15RwOpCombineType"));
    SET_TO(TextureDatabaseLoad,             aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime4LoadEPKcb21TextureDatabaseFormat"));
    SET_TO(TextureDatabaseRegister,         aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime8RegisterEPS_"));
    SET_TO(TextureDatabaseUnregister,       aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime10UnregisterEPS_"));
    SET_TO(TextureDatabaseGetTexture,       aml->GetSym(hGTASA, "_ZN22TextureDatabaseRuntime10GetTextureEPKc"));
    SET_TO(DeActivateDirectional,           aml->GetSym(hGTASA, "_Z21DeActivateDirectionalv"));
    SET_TO(SetAmbientColours,               aml->GetSym(hGTASA, "_Z17SetAmbientColoursP10RwRGBAReal"));
    SET_TO(SetFullAmbient,                  aml->GetSym(hGTASA, "_Z14SetFullAmbientv"));
    SET_TO(RwRenderStateSet,                aml->GetSym(hGTASA, "_Z16RwRenderStateSet13RwRenderStatePv"));
    
    // GTA Hooks
    HOOKPLT(GameInit3,                      pGTASA + BYVER(0x6742F0, 0x8470F0));
    HOOKPLT(RenderClouds,                   pGTASA + BYVER(0x672FFC, 0x8451A0));
    #ifdef AML32
        HOOKPLT(GameLogicPassTime,          pGTASA + 0x66F620);
    #else
        HOOK(GameLogicPassTime,             aml->GetSym(hGTASA, "_ZN10CGameLogic8PassTimeEj"));
    #endif
}

extern "C" void OnModLoad()
{
    // This one below doesnt work. Yet?
    minFarPlane = cfg->GetFloat("MinDrawDistance", minFarPlane, "Game tweaks");
    if(minFarPlane > 300)
    {
        HOOKPLT(TimecycUpdate,              pGTASA + BYVER(0x673A5C, 0x8462B0));
        #ifdef AML32
            MinFarClip_Continue =           pGTASA + 0x5A3598 + 0x1;
            MinFarClip_Break =              pGTASA + 0x5A368A + 0x1;
            aml->Redirect(pGTASA + 0x5A3680 + 0x1, (uintptr_t)MinFarClip_Patch);
            HOOKBLX(SetRenderState_SunRefl, pGTASA + 0x5A36A4 + 0x1);
        #else
            MinFarClip_Continue =           pGTASA + 0x6C6E00;
            MinFarClip_Break =              pGTASA + 0x6C6EC4;
            aml->Redirect(pGTASA + 0x6C6EB4, (uintptr_t)MinFarClip_Patch);
            HOOKBL(SetRenderState_SunRefl, pGTASA + 0x6C6EEC);
        #endif
    }
    if(cfg->GetBool("NoLowClouds", false, "Game tweaks"))
    {
        aml->Redirect(aml->GetSym(hGTASA, "_ZN7CClouds22VolumetricCloudsRenderEv"), (uintptr_t)Stub); // Yes, JuniorDjjr switched them by an accident. Keeping order for configs.
    }
    if(cfg->GetBool("NoHorizonClouds", false, "Game tweaks"))
    {
        aml->PlaceB(pGTASA + BYVER(0x59F338 + 0x1, 0x6C32D4), pGTASA + BYVER(0x59F40A + 0x1, 0x6C3380));
    }
    if(cfg->GetBool("NoVolumetricClouds", false, "Game tweaks"))
    {
        aml->Redirect(aml->GetSym(hGTASA, "_ZN7CClouds22RenderBottomFromHeightEv"), (uintptr_t)Stub); // Yes, JuniorDjjr switched them by an accident. Keeping order for configs.
    }
    if(cfg->GetBool("NoVanillaStars", false, "Game tweaks"))
    {
        aml->PlaceB(pGTASA + BYVER(0x59F002 + 0x1, 0x6C2FDC), pGTASA + BYVER(0x59F20A + 0x1, 0x6C30EC));
    }
    
    skyboxDrawAfter = cfg->GetBool("SkyboxDrawAfter", skyboxDrawAfter, "Skybox");
    skyboxFogType = cfg->GetInt("SkyboxFogType", skyboxFogType, "Skybox");
    fogDensityDefault = cfg->GetFloat("SkyboxFogDistance", fogDensityDefault, "Skybox");
    cloudsRotationSpeed = cfg->GetFloat("CloudsRotationSpeed", cloudsRotationSpeed, "Skybox");
    starsRotationSpeed = cfg->GetFloat("StarsRotationSpeed", starsRotationSpeed, "Skybox");
    skyboxSizeXY = cfg->GetFloat("SkyboxSizeXY", skyboxSizeXY, "Skybox");
    skyboxSizeZ = cfg->GetFloat("SkyboxSizeZ", skyboxSizeZ, "Skybox");
    cloudsMultBrightness = cfg->GetFloat("CloudsMultBrightness", cloudsMultBrightness, "Skybox");
    cloudsNightDarkLimit = cfg->GetFloat("CloudsNightDarkLimit", cloudsNightDarkLimit, "Skybox");
    cloudsMinBrightness = cfg->GetFloat("CloudsMinBrightness", cloudsMinBrightness, "Skybox");
    cloudsMultSunrise = cfg->GetFloat("CloudsMultSunrise", cloudsMultSunrise, "Skybox");
    cloudsCityOrange = cfg->GetFloat("CloudsCityOrange", cloudsCityOrange, "Skybox");
    starsCityAlphaRemove = cfg->GetFloat("StarsCityAlphaRemove", starsCityAlphaRemove, "Skybox");

    // A new feature
    windStrengthAffectsRotation = cfg->GetBool("WindAffectsRotationSpeed", windStrengthAffectsRotation, "Skybox");
    windRotScale = cfg->GetFloat("WindRotationScale", windRotScale, "Skybox");

    skyScaleAsOnPC = cfg->GetBool("ScaleSkiesAsOnPC", skyScaleAsOnPC, "Skybox");
}