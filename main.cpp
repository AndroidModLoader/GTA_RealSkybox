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
RpAtomicCallBackRender AtomicDefaultRenderCallBack;

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
void            (*CFileMgr__SetDir)(const char *dir);
int             (*CFileMgr__OpenFile)(const char *path, const char *mode);
void            (*CFileMgr__CloseFile)(int fd);
char*           (*CFileLoader__LoadLine)(int fd);
void            (*RwFrameTranslate)(RwFrame*, CVector*, RwOpCombineType);
void            (*RwFrameScale)(RwFrame*, CVector*, RwOpCombineType);

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
RwTexture* SAUtils__LoadRwTextureFromPNG(const char* fn)
{
    RwTexture* pTexture = NULL;
    if (RwImage* pImage = RtPNGImageRead(fn))
    {
        int width, height, depth, flags;
        RwImageFindRasterFormat(pImage, rwRASTERTYPETEXTURE, &width, &height, &depth, &flags);
        if (RwRaster* pRaster = RwRasterCreate(width, height, depth, flags))
        {
            RwRasterSetFromImage(pRaster, pImage);
            pTexture = RwTextureCreate(pRaster);
        }
        RwImageDestroy(pImage);
    }
    return pTexture;
}
void LoadSkyboxTextures()
{
    const char* szStartPath = "realskybox/tex/";

    CFileMgr__SetDir("REALSKYBOX");
    int fd = CFileMgr__OpenFile("SKYBOXES.DAT", "rb");
    CFileMgr__SetDir("");

    const char* line = NULL;
    bool bStartLoading = false;
    int weatherId = 0;
    char textureName[32];
    while((line = CFileLoader__LoadLine(fd)) != NULL)
    {
        if(line[0] == 0 || line[0] == '#' || line[0] == ';' || line[0] == '/') continue;
        if(!bStartLoading)
        {
            if(!strncmp(line, "skytexs", 8)) bStartLoading = true;
            continue;
        }

        if(sscanf(line, "%d, %s", &weatherId, (char*)&textureName) == 2 && weatherId <= WEATHER_FOR_STARS)
        {
            char texNamePath[64];
            sprintf(texNamePath, "%s%s.png", szStartPath, textureName);
            aSkyboxes[weatherId]->tex = SAUtils__LoadRwTextureFromPNG(texNamePath);
            if (aSkyboxes[weatherId]->tex) aSkyboxes[weatherId]->tex->filterAddressing = 2;
        }
    }

    CFileMgr__CloseFile(fd);
}
void PrepareSkyboxModel()
{
    RwStream *stream = RwStreamOpen(2, 1, "realskybox/skybox.dff");
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
void RenderSkybox()
{
    RwFrameTranslate(pSkyFrame, &TheCamera->GetPosition(), rwCOMBINEREPLACE);
    CVector scale = CVector(0.3f, 0.3f, 0.3f);
    RwFrameScale(pSkyFrame, &scale, rwCOMBINEPRECONCAT);
    //RwFrameRotate(pSkyFrame, (RwV3d*)0x008D2E18, aSkyboxes[newWeatherType]->rot, rwCOMBINEPRECONCAT);
    RwFrameUpdateObjects(pSkyFrame);
    pSkyAtomic->geometry->matList.materials[0]->texture = aSkyboxes[0]->tex;
    RenderAtomicWithAlpha(pSkyAtomic, 255);
}

/////////////////////////////////////////////////////////////////////////////
///////////////////////////////     Hooks     ///////////////////////////////
/////////////////////////////////////////////////////////////////////////////
DECL_HOOKv(GameInit3, void* data)
{
    GameInit3(data);

    for (int i = 0; i < 21; ++i) { aSkyboxes[i] = new Skybox(); }

    LoadSkyboxTextures();
    PrepareSkyboxModel();
}
DECL_HOOKv(RenderClouds)
{
    RenderClouds();
    RenderSkybox();
}

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
    SET_TO(CFileMgr__SetDir,                aml->GetSym(hGTASA, "_ZN8CFileMgr6SetDirEPKc"));
    SET_TO(CFileMgr__OpenFile,              aml->GetSym(hGTASA, "_ZN8CFileMgr8OpenFileEPKcS1_"));
    SET_TO(CFileMgr__CloseFile,             aml->GetSym(hGTASA, "_ZN8CFileMgr9CloseFileEj"));
    SET_TO(CFileLoader__LoadLine,           aml->GetSym(hGTASA, "_ZN11CFileLoader8LoadLineEj"));
    SET_TO(RwFrameTranslate,                aml->GetSym(hGTASA, "_Z16RwFrameTranslateP7RwFramePK5RwV3d15RwOpCombineType"));
    SET_TO(RwFrameScale,                    aml->GetSym(hGTASA, "_Z12RwFrameScaleP7RwFramePK5RwV3d15RwOpCombineType"));

    // GTA Patches
    //_BackTo = pGTASA + 0x + 0x1;
    //aml->Redirect(pGTASA + 0x + 0x1, (uintptr_t)_inject);
    
    // GTA Hooks
    HOOK(GameInit3,                         aml->GetSym(hGTASA, "_ZN5CGame5Init3EPKc"));
    HOOK(RenderClouds,                      aml->GetSym(hGTASA, "_ZN7CClouds6RenderEv"));
}