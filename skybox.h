class Skybox
{
public:
    RwTexture *tex;
    float rot;
    bool inUse;

    Skybox()
    {
        inUse = false;
        tex =   NULL;
        rot =   0.0f;
    }
};