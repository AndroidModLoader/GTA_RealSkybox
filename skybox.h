class Skybox
{
public:
    bool inUse;
    RwTexture *tex;
    float rot;
    Skybox()
    {
        inUse = false;
        tex = NULL;
        rot = 0.0f;
    }
};