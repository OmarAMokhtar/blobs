/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// OpenGL ES 2.0 code

#include <jni.h>
#include <android/log.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "3ds.h"
#include "texture.h"

#define  LOG_TAG    "libgl2jni"
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#define CHK checkGlError(__FILE__,__LINE__)

static void printGLString(const char *name, GLenum s)
{
    const char *v = (const char *) glGetString(s);
    LOGI("GL %s = %s\n", name, v);
}

static void checkGlError(const char* file, int line)
{
    for (GLint error = glGetError(); error ; error = glGetError())
    {
        LOGI("Error (0x%x) at %s %d\n", error, file, line);
    }
}

glm::mat4 Projection;

#define NUM_VERTICES 2000000
#define NUM_INDICES  2000000

static GLfloat gVertexList[NUM_VERTICES*3];
static GLfloat gTexturesUVList[NUM_VERTICES*2];
static GLfloat gColorList[NUM_VERTICES*3];
static GLfloat gNormalList[NUM_VERTICES*3];
static GLfloat gUseTextures[NUM_VERTICES];
static GLfloat gSamplerList[NUM_VERTICES];
static unsigned short gIndicesList[NUM_INDICES];

static int gNumVertexList = 0;
static int gNumIndicesList = 0;

CLoad3DS sModelsLoader;

struct TextureInfo
{
    char filename[50];
    int width;
    int height;
    char* data;
    GLuint textureID;
};
static TextureInfo gTextureList[30];
static int gNumTextureList = 0;

struct ModelArrayInfo
{
    int vertexOffset;
    int numVertices;
    int indexOffset;
    int numIndices;
    GLuint vertexbuffer;
    GLuint indicesbuffer;
    GLuint uvbuffer;
    GLfloat max_x;
    GLfloat min_x;
    GLfloat max_y;
    GLfloat min_y;
    GLfloat max_z;
    GLfloat min_z;
    GLuint colorbuffer;
    GLuint normalbuffer;
    GLuint samplerbuffer;
    GLuint usetexbuffer;
    GLuint sampler_map[32];
    int num_sampler_map;
};
static ModelArrayInfo gModelArrayInfos[20];
static int gNumModelArrayInfos = 0;

volatile int gTotalBytes = 100;
volatile int gLoadedBytes = 0;
volatile unsigned char gLoadingPercent = 0;

int getTimeNsec()
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_nsec;
}

auto gVertexShader =
    "#version 100\n"
    "attribute vec3 vPosition;\n"
    "attribute vec3 vNormal;\n"
    "attribute vec3 vColor;\n"
    "attribute vec2 vTextureUV;\n"
    "attribute float vUseTexture;\n"
    "attribute float vSamplerID;\n"
    "uniform mediump vec3 LightPosition_worldspace;\n"
    "uniform float lightPower;\n"
    "uniform mediump vec3 LightColor;\n"
    "uniform mat4 mvp;\n"
    "uniform mat4 v;\n"
    "uniform mat4 m;\n"
    "varying vec3 fragmentColor;\n"
    "varying vec3 Normal_cameraspace;\n"
    "varying vec3 Position_worldspace;\n"
    "varying vec3 EyeDirection_cameraspace;\n"
    "varying vec3 LightDirection_cameraspace;\n"
    "varying float SamplerID;\n"
    "varying float UseTexture;\n"
    "varying vec2 UV;\n"
    "void main() {\n"
    "  vec4 vPosition4 = vec4(vPosition,1.0);\n"
    "  gl_Position = mvp*vPosition4;\n"
    "  Position_worldspace = (m*vPosition4).xyz;\n"
    "  vec3 vertexPosition_cameraspace = (v*m*vPosition4).xyz;\n"
    "  EyeDirection_cameraspace = vec3(0.0, 0.0, 0.0) - vertexPosition_cameraspace;\n"
    "  vec3 LightPosition_cameraspace = (v*vec4(LightPosition_worldspace, 1.0)).xyz;\n"
    "  LightDirection_cameraspace = LightPosition_cameraspace + EyeDirection_cameraspace;\n"
    "  Normal_cameraspace = (v*m*vec4(vNormal, 0.0)).xyz;\n"
    "  fragmentColor = vColor;\n"
    "  UV = vTextureUV;\n"
    "  UseTexture = vUseTexture;\n"
    "  SamplerID = vSamplerID;\n"
    "}\n";

auto gFragmentShader =
    "#version 100\n"
    "precision mediump float;\n"
    "uniform mediump vec3 LightPosition_worldspace;\n"
    "uniform mediump vec3 LightColor;\n"
    "uniform float lightPower;\n"
    "uniform sampler2D vSamplersArray[32];\n"
    "varying vec3 fragmentColor;\n"
    "varying vec3 Position_worldspace;\n"
    "varying vec3 Normal_cameraspace;\n"
    "varying vec3 EyeDirection_cameraspace;\n"
    "varying vec3 LightDirection_cameraspace;\n"
    "varying float SamplerID;\n"
    "varying float UseTexture;\n"
    "varying vec2 UV;\n"
    "void main() {\n"
    "  mediump int index = int(SamplerID);\n"
    "  vec3 MaterialDiffuseColor = (1.0-UseTexture)*fragmentColor + UseTexture*texture2D(vSamplersArray[index],UV).rgb;\n"
    "  vec3 MaterialAmbientColor = vec3(0.1, 0.1, 0.1)*MaterialDiffuseColor;\n"
    "  vec3 MaterialSpecularColor = vec3(0.3, 0.3, 0.3);\n"
    "  float distance = length(LightPosition_worldspace-Position_worldspace);\n"
    "  vec3 n = normalize(Normal_cameraspace);\n"
    "  vec3 l = normalize(LightDirection_cameraspace);\n"
    "  float cosTheta = clamp(dot(n,l),0.0,1.0);\n"
    "  vec3 E = normalize(EyeDirection_cameraspace);\n"
    "  vec3 R = reflect(-l,n);\n"
    "  float cosAlpha = clamp(dot(E,R),0.0,1.0);\n"
    "  gl_FragColor.rgb =\n"
    "    MaterialAmbientColor +\n"
    "    MaterialDiffuseColor*LightColor*lightPower*cosTheta/(distance*distance) +\n"
    "    MaterialSpecularColor*LightColor*lightPower*pow(cosAlpha, 5.0)/(distance*distance);\n"
    "}\n";

GLuint loadShader(GLenum shaderType, const char* pSource)
{
    GLuint shader = glCreateShader(shaderType); CHK;
    if (shader)
    {
        glShaderSource(shader, 1, &pSource, NULL); CHK;
        glCompileShader(shader); CHK;
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen); CHK;
            if (infoLen)
            {
                char* buf = (char*) malloc(infoLen);
                if (buf)
                {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf); CHK;
                    LOGE("Could not compile shader %d:\n%s\n",shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader); CHK;
                shader = 0;
            }
        }
    }
    return shader;
}

GLuint createProgram(const char* pVertexSource, const char* pFragmentSource)
{
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader) return 0;

    GLuint pixelShader = loadShader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader) return 0;

    GLuint program = glCreateProgram(); CHK;
    if (program)
    {
        glAttachShader(program, vertexShader); CHK;
        glAttachShader(program, pixelShader); CHK;
        glLinkProgram(program); CHK;
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus); CHK;
        if (linkStatus != GL_TRUE)
        {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength); CHK;
            if (bufLength)
            {
                char* buf = (char*) malloc(bufLength);
                if (buf)
                {
                    glGetProgramInfoLog(program, bufLength, NULL, buf); CHK;
                    LOGE("Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program); CHK;
            program = 0;
        }
    }
    return program;
}

GLuint gProgram;
GLuint gvPositionHandle;
GLuint gvTransHandle;
GLuint gvUseTextureHandle;
GLuint gvTextureUVHandle;
GLuint gvSamplerHandle;
GLuint gvSamplersArrayHandle;
GLuint gLightPos;
GLuint gLightPower;
GLuint gLightColor;
GLuint gvVMatHandle;
GLuint gvMMatHandle;
GLuint gvColorHandle;
GLuint gvNormalHandle;

bool resize(int w, int h)
{
    Projection = glm::perspective(glm::radians(90.0f), (float)w/(float)h, 0.1f, 100.0f);
    glViewport(0, 0, w, h); CHK;
    return true;
}

static int lastTime = 0;

void createTextures(int i)
{
    glGenTextures(1, &gTextureList[i].textureID); CHK;
    glBindTexture(GL_TEXTURE_2D, gTextureList[i].textureID); CHK;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, gTextureList[i].width, gTextureList[i].height, 0, GL_RGB, GL_UNSIGNED_BYTE, gTextureList[i].data); CHK;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); CHK;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); CHK;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); CHK;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR); CHK;
    glGenerateMipmap(GL_TEXTURE_2D); CHK;
}

void createBuffersForModel(int i)
{
    glGenBuffers(1, &gModelArrayInfos[i].vertexbuffer); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, gModelArrayInfos[i].vertexbuffer); CHK;
    glBufferData(GL_ARRAY_BUFFER, gModelArrayInfos[i].numVertices*sizeof(GLfloat), &gVertexList[gModelArrayInfos[i].vertexOffset], GL_STATIC_DRAW); CHK;

    glGenBuffers(1, &gModelArrayInfos[i].indicesbuffer); CHK;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gModelArrayInfos[i].indicesbuffer); CHK;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, gModelArrayInfos[i].numIndices*sizeof(unsigned short), &gIndicesList[gModelArrayInfos[i].indexOffset], GL_STATIC_DRAW); CHK;

    glGenBuffers(1, &gModelArrayInfos[i].uvbuffer); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, gModelArrayInfos[i].uvbuffer); CHK;
    glBufferData(GL_ARRAY_BUFFER, (gModelArrayInfos[i].numVertices/3)*2*sizeof(GLfloat), &gTexturesUVList[(gModelArrayInfos[i].vertexOffset/3)*2], GL_STATIC_DRAW); CHK;

    glGenBuffers(1, &gModelArrayInfos[i].colorbuffer); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, gModelArrayInfos[i].colorbuffer); CHK;
    glBufferData(GL_ARRAY_BUFFER, gModelArrayInfos[i].numVertices*sizeof(GLfloat), &gColorList[gModelArrayInfos[i].vertexOffset], GL_STATIC_DRAW); CHK;

    glGenBuffers(1, &gModelArrayInfos[i].normalbuffer); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, gModelArrayInfos[i].normalbuffer); CHK;
    glBufferData(GL_ARRAY_BUFFER, gModelArrayInfos[i].numVertices*sizeof(GLfloat), &gNormalList[gModelArrayInfos[i].vertexOffset], GL_STATIC_DRAW); CHK;

    glGenBuffers(1, &gModelArrayInfos[i].samplerbuffer); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, gModelArrayInfos[i].samplerbuffer); CHK;
    glBufferData(GL_ARRAY_BUFFER, (gModelArrayInfos[i].numVertices/3)*sizeof(GLfloat), &gSamplerList[gModelArrayInfos[i].vertexOffset/3], GL_STATIC_DRAW); CHK;

    glGenBuffers(1, &gModelArrayInfos[i].usetexbuffer); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, gModelArrayInfos[i].usetexbuffer); CHK;
    glBufferData(GL_ARRAY_BUFFER, (gModelArrayInfos[i].numVertices/3)*sizeof(GLfloat), &gUseTextures[gModelArrayInfos[i].vertexOffset/3], GL_STATIC_DRAW); CHK;
}

void PrepareModelToBeDrawn(ModelArrayInfo &model_info, glm::vec3 light_pos, glm::vec3 light_color, float light_power)
{
    glUseProgram(gProgram); CHK;

    GLint samplers_array[32] = {0};
    for (int i = 0 ; i < model_info.num_sampler_map ; i++)
    {
        glActiveTexture(GL_TEXTURE0+i); CHK;
        glBindTexture(GL_TEXTURE_2D,gTextureList[model_info.sampler_map[i]].textureID); CHK;
        samplers_array[i] = i;
    }
    glUniform1iv(gvSamplersArrayHandle,model_info.num_sampler_map,&samplers_array[0]); CHK;

    glEnableVertexAttribArray(gvPositionHandle); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, model_info.vertexbuffer); CHK;
    glVertexAttribPointer(gvPositionHandle, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); CHK;
    glEnableVertexAttribArray(gvColorHandle); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, model_info.colorbuffer); CHK;
    glVertexAttribPointer(gvColorHandle, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); CHK;
    glEnableVertexAttribArray(gvNormalHandle); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, model_info.normalbuffer); CHK;
    glVertexAttribPointer(gvNormalHandle, 3, GL_FLOAT, GL_FALSE, 0, (void*)0); CHK;
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, model_info.indicesbuffer); CHK;
    glEnableVertexAttribArray(gvUseTextureHandle); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, model_info.usetexbuffer); CHK;
    glVertexAttribPointer(gvUseTextureHandle, 1, GL_FLOAT, GL_FALSE, 0, (void*)0); CHK;
    glEnableVertexAttribArray(gvSamplerHandle); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, model_info.samplerbuffer); CHK;
    glVertexAttribPointer(gvSamplerHandle, 1, GL_FLOAT, GL_FALSE, 0, (void*)0); CHK;
    glEnableVertexAttribArray(gvTextureUVHandle); CHK;
    glBindBuffer(GL_ARRAY_BUFFER, model_info.uvbuffer); CHK;
    glVertexAttribPointer(gvTextureUVHandle, 2, GL_FLOAT, GL_FALSE, 0, (void*)0); CHK;

    glUniform3fv(gLightPos, 1, &light_pos[0]); CHK;
    glUniform3fv(gLightColor, 1, &light_color[0]); CHK;
    glUniform1f(gLightPower, light_power); CHK;
}

void DrawModel(ModelArrayInfo &model_info, glm::mat4 Model, glm::mat4 View)
{
    glm::mat4 mvp = Projection * View * Model;
    glUniformMatrix4fv(gvVMatHandle, 1, GL_FALSE, &View[0][0]); CHK;
    glUniformMatrix4fv(gvTransHandle, 1, GL_FALSE, &mvp[0][0]); CHK;
    glUniformMatrix4fv(gvMMatHandle, 1, GL_FALSE, &Model[0][0]); CHK;
    glDrawElements(GL_TRIANGLES,model_info.numIndices,GL_UNSIGNED_SHORT,(void*)0); CHK;
}

void UnprepareModel(ModelArrayInfo &model_info)
{
    glDisableVertexAttribArray(gvPositionHandle); CHK;
    glDisableVertexAttribArray(gvColorHandle); CHK;
    glDisableVertexAttribArray(gvNormalHandle); CHK;
    glDisableVertexAttribArray(gvUseTextureHandle); CHK;
    glDisableVertexAttribArray(gvSamplerHandle); CHK;
    glDisableVertexAttribArray(gvTextureUVHandle); CHK;
}

bool setupGraphics()
{
    glEnable(GL_DEPTH_TEST); CHK;
    glDepthFunc(GL_LESS); CHK;
    glEnable(GL_TEXTURE_2D); CHK;

    gProgram = createProgram(gVertexShader, gFragmentShader);
    if (!gProgram)
    {
        LOGE("Could not create program");
        return false;
    }

    gvTransHandle = (GLuint)glGetUniformLocation(gProgram, "mvp"); CHK;
    gvVMatHandle = (GLuint)glGetUniformLocation(gProgram, "v"); CHK;
    gvMMatHandle = (GLuint)glGetUniformLocation(gProgram, "m"); CHK;
    gLightPos = (GLuint)glGetUniformLocation(gProgram, "LightPosition_worldspace"); CHK;
    gLightPower = (GLuint)glGetUniformLocation(gProgram, "lightPower"); CHK;
    gLightColor = (GLuint)glGetUniformLocation(gProgram, "LightColor"); CHK;
    gvPositionHandle = (GLuint)glGetAttribLocation(gProgram, "vPosition"); CHK;
    gvColorHandle = (GLuint)glGetAttribLocation(gProgram, "vColor"); CHK;
    gvNormalHandle = (GLuint)glGetAttribLocation(gProgram, "vNormal"); CHK;
    gvTextureUVHandle = (GLuint)glGetAttribLocation(gProgram, "vTextureUV"); CHK;
    gvUseTextureHandle = (GLuint)glGetAttribLocation(gProgram, "vUseTexture"); CHK;
    gvSamplerHandle = (GLuint)glGetAttribLocation(gProgram, "vSamplerID"); CHK;
    gvSamplersArrayHandle = (GLuint)glGetUniformLocation(gProgram, "vSamplersArray"); CHK;

    createTextures(0);
    createBuffersForModel(0);

    lastTime = getTimeNsec();

    return true;
}

enum GameState { LOADING_SCREEN, CHOOSE_LEVEL, PLAYING_GAME };
static volatile GameState game_state = LOADING_SCREEN;
enum LoadingState { LOADING_TEXTURES, BINDING_TEXTURES, LOADING_MODELS, BINDING_MODELS, FINISHED_LOADING };
static volatile LoadingState loading_state = LOADING_TEXTURES;

static float player_angle = 0;
static float player_x = 0;
static float player_y = 0;

struct ItemInMap
{
    glm::vec3 position;
    int model_id;
};

struct MapSection
{
    ItemInMap items[10];
    int numItems;
};

static MapSection game_map[10][10];

inline void GetMapSection(float x, float y, int &x_id, int &y_id)
{
    x -= 10;
    y -= 10;
    x *= 0.5;
    y *= 0.5;
    x_id = (int)x;
    y_id = (int)y;
}

void renderFrame(float dx, float dy, float dangle, float scale)
{
    /* Handling Delta Time */
    static float timestamp = 0;
    static float finished_loading_timestamp = 0;
    float delta_time;
    int now = getTimeNsec();
    if (lastTime < now) delta_time = (now-lastTime)/1000000000.0f;
    else delta_time = (now+1000000000-lastTime)/1000000000.0f;

    static int cntr = 0;
    cntr++;
    if (cntr%10 == 0)
    {
        LOGI("Frame Rate %f\n",1.0/delta_time);
    }

    if (delta_time > 0.5)
    {
        delta_time = 0.5;
    }
    timestamp += delta_time;
    lastTime = now;

    /* Check Screen */
    switch (game_state)
    {
        case LOADING_SCREEN:
        {
            float angle;
            int time_int = (int)timestamp;
            float frac = timestamp - time_int;
            float division = 1+timestamp*0.1f;
            if (time_int%2) angle = -45+frac*90;
            else angle = +45-frac*90;
            float light_factor = timestamp;

            static int internal_texture_counter = 1;
            static int internal_model_counter = 1;

            ModelArrayInfo model_info = gModelArrayInfos[0];

            float close_up = 0;

            switch (loading_state)
            {
                case LOADING_TEXTURES:
                    internal_texture_counter = 1;
                    break;

                case BINDING_TEXTURES:
                    createTextures(internal_texture_counter);
                    internal_texture_counter++;
                    if (internal_texture_counter >= gNumTextureList)
                        loading_state = (volatile LoadingState)LOADING_MODELS;
                    break;

                case LOADING_MODELS:
                    internal_model_counter = 1;
                    break;

                case BINDING_MODELS:
                    createBuffersForModel(internal_model_counter);
                    internal_model_counter++;
                    if (internal_model_counter >= gNumModelArrayInfos)
                    {
                        finished_loading_timestamp = timestamp;
                        loading_state = (volatile LoadingState)FINISHED_LOADING;
                        LOGI("MAXES %d %d\n",gNumVertexList/3,gNumIndicesList);
                    }
                    break;

                case FINISHED_LOADING:
                    close_up = pow((timestamp-finished_loading_timestamp),3);
                    if (close_up > 20)
                    {
                        timestamp = 0;
                        game_state = (volatile GameState)CHOOSE_LEVEL;
                    }
                    break;
            }

            glClearColor(1.0, 1.0, 0.0, 1.0f); CHK;
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT); CHK;

            glm::mat4 Model = glm::scale(glm::vec3(0.003,0.003,0.003));
            Model = glm::rotate(Model,-40.0f-close_up*2.5f,glm::vec3(0,1,0));
            Model = glm::rotate(Model,timestamp*60,glm::vec3(0,0,1));
            glm::mat4 View = glm::lookAt(glm::vec3(0,10-close_up/2,40-close_up*1.5),glm::vec3(0,0,0),glm::vec3(0,1,0));

            PrepareModelToBeDrawn(model_info,glm::vec3(0.0, -30, 10),glm::vec3(1.0, 1.0, 1.0),1600+fabs(800*cos(light_factor*10)));
            DrawModel(model_info,Model,View);
            UnprepareModel(model_info);

            break;
        }

        default:
        {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f); CHK;
            glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT); CHK;

//            player_angle += dx;
//            float the_angle = glm::radians(player_angle);
//            float the_sin = sin(the_angle);
//            float the_cos = -cos(the_angle);

//            static int whatever = 0;
//            whatever++;
//            if (whatever%20 == 0)
//                LOGI("%f %f %f\n",player_x,player_y,player_angle);

//            glm::vec3 eye = glm::vec3(player_x-15, 15, player_y-15);
//            glm::vec3 dir = glm::vec3(15, -15.0, 15);
//            glm::mat4 View = glm::lookAt(eye, eye+dir, glm::vec3(0,1,0));

            static float scaling_factor = 1.0;
            static float rotation_angle = 0;

            static int mycntr = 0;
            mycntr++;
            if (mycntr%20 == 0)
                LOGI("dx,dy,dangle = %f,%f,%f\n",dx,dy,dangle);

            player_angle += dx*100;

            glm::mat4 View = glm::lookAt(glm::vec3(20,20,20),glm::vec3(0,0,0),glm::vec3(0,1,0));

//            ModelArrayInfo model_info = gModelArrayInfos[2];
//            PrepareModelToBeDrawn(model_info, glm::vec3(20, 20, 20), glm::vec3(1.0, 1.0, 1.0),2000);
//            glm::mat4 Model = glm::scale(glm::vec3(0.1, 0.1, 0.1));
//            DrawModel(model_info, Model, View);
//            UnprepareModel(model_info);
//
//            model_info = gModelArrayInfos[3];
//            PrepareModelToBeDrawn(model_info, glm::vec3(20, 20, 20), glm::vec3(1.0, 1.0, 1.0),2000);
//            Model = glm::translate(glm::vec3(0.6,0,0.6))*glm::scale(glm::vec3(0.1, 0.1, 0.1));
//            DrawModel(model_info, Model, View);
//            UnprepareModel(model_info);

            ModelArrayInfo model_info = gModelArrayInfos[9];
            PrepareModelToBeDrawn(model_info, glm::vec3(20, 20, 20), glm::vec3(1.0, 1.0, 1.0),1000);
            glm::mat4 Model = glm::scale(glm::vec3(0.0003, 0.0003, 0.0003));
            DrawModel(model_info, Model, View);
            UnprepareModel(model_info);

            break;
        }
    }
}

extern "C" {
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj,  jfloat dx, jfloat dy, jfloat dangle, jfloat scale);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_setTotalBytes(JNIEnv * env, jobject obj, jint total);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_resize(JNIEnv * env, jobject obj,  jint width, jint height);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadModel(JNIEnv * env, jobject obj, jstring name, jbyteArray buffer, jboolean external);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadBMP(JNIEnv * env, jobject obj, jstring filename, jbyteArray buffer);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadDDS(JNIEnv * env, jobject obj, jstring filename, jbyteArray buffer);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadTGA(JNIEnv * env, jobject obj, jstring filename, jbyteArray buffer);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadRAW(JNIEnv * env, jobject obj, jstring filename, jint width, jint height, jint src_size, jintArray buffer);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_doneLoadingTextures(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_doneLoadingModels(JNIEnv * env, jobject obj);
};

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_init(JNIEnv * env, jobject obj)
{
    setupGraphics();
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_resize(JNIEnv * env, jobject obj,  jint width, jint height)
{
    resize(width, height);
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_step(JNIEnv * env, jobject obj,  jfloat dx, jfloat dy, jfloat dangle, jfloat scale)
{
    if (gNumVertexList > 0)
        renderFrame(dx,dy,dangle,scale);
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadModel(JNIEnv * env, jobject obj,  jstring name, jbyteArray buffer, jboolean external)
{
    jboolean isCopy = 0;
    jbyte* data = env->GetByteArrayElements(buffer, &isCopy);
    int buffer_size = env->GetArrayLength(buffer);

    LOGI("Loading Model[%d] %s\n",gNumModelArrayInfos,env->GetStringUTFChars(name,&isCopy));

    gModelArrayInfos[gNumModelArrayInfos].indexOffset = gNumIndicesList;
    gModelArrayInfos[gNumModelArrayInfos].vertexOffset = gNumVertexList;

    t3DModel model;
    memset(&model,0,sizeof(model));
    sModelsLoader.Import3DS(&model,(const char*)data);

    gLoadedBytes += buffer_size;
    gLoadingPercent = (gLoadedBytes*100)/gTotalBytes;

    float max_x = model.pObject[0].pVerts[0].x;
    float min_x = model.pObject[0].pVerts[0].x;
    float max_y = model.pObject[0].pVerts[0].y;
    float min_y = model.pObject[0].pVerts[0].y;
    float max_z = model.pObject[0].pVerts[0].z;
    float min_z = model.pObject[0].pVerts[0].z;

    gModelArrayInfos[gNumModelArrayInfos].num_sampler_map = 0;
    memset(gModelArrayInfos[gNumModelArrayInfos].sampler_map,0,sizeof(gModelArrayInfos[gNumModelArrayInfos].sampler_map));

    int last_offset = 0;
    if (model.numOfMaterials == 0 || external)
    {
        external = true;

        char col_tex[50];
        sprintf(col_tex, "%s_col", env->GetStringUTFChars(name, &isCopy));

        bool found = false;
        for (int i = 0; i < gNumTextureList; i++)
        {
            if (!strcasecmp(col_tex, gTextureList[i].filename))
            {
                gModelArrayInfos[gNumModelArrayInfos].sampler_map[0] = i;
                gModelArrayInfos[gNumModelArrayInfos].num_sampler_map = 1;
                found = true;
                break;
            }
        }
        if (!found)
        {
            LOGI("%s NOT FOUND!\n", col_tex);
        }
    }
    else
    {
        for (int m = 0 ; m < model.numOfMaterials ; m++)
        {
            bool found = false;
            model.pMaterials[m].texureId = 0;
            for (int i = 0 ; i < gNumTextureList ; i++)
            {
                if (!strcasecmp(model.pMaterials[m].strFile,gTextureList[i].filename))
                {
                    model.pMaterials[m].texureId = i;
                    found = true;
                    break;
                }
            }
            if (found)
            {
                bool found_sampler = false;
                for (int i = 0 ; i < gModelArrayInfos[gNumModelArrayInfos].num_sampler_map ; i++)
                {
                    if (gModelArrayInfos[gNumModelArrayInfos].sampler_map[i] == model.pMaterials[m].texureId)
                    {
                        found_sampler = true;
                    }
                }
                if (!found_sampler)
                {
                    gModelArrayInfos[gNumModelArrayInfos].sampler_map[gModelArrayInfos[gNumModelArrayInfos].num_sampler_map++] = model.pMaterials[m].texureId;
                }

                LOGI("Material %d: File %s Name %s Color %d %d %d Texture %d\n", m,
                     model.pMaterials[m].strFile,
                     model.pMaterials[m].strName,
                     model.pMaterials[m].color[0],
                     model.pMaterials[m].color[2],
                     model.pMaterials[m].color[3],
                     model.pMaterials[m].texureId);
            }
            else
            {
                if (strlen(model.pMaterials[m].strFile) > 0)
                    LOGI("Texture %s NOT FOUND!\n",model.pMaterials[m].strFile);
            }
        }
    }

    for (int i = 0 ; i < model.numOfObjects ; i++)
    {
        int material = -1;
        int obj_sampler = -1;
        if (!external)
        {
            material = model.pObject[i].materialID;
            for (int s = 0 ; s < gModelArrayInfos[gNumModelArrayInfos].num_sampler_map ; s++)
            {
                if (model.pMaterials[material].texureId == gModelArrayInfos[gNumModelArrayInfos].sampler_map[s])
                {
                    obj_sampler = s;
                }
            }
        }
        else
        {
            obj_sampler = 0;
        }

        for (int vindx = 0 ; vindx < model.pObject[i].numOfVerts ; vindx++)
        {
            gVertexList[gNumVertexList] = model.pObject[i].pVerts[vindx].x;
            gVertexList[gNumVertexList+1] = model.pObject[i].pVerts[vindx].y;
            gVertexList[gNumVertexList+2] = model.pObject[i].pVerts[vindx].z;

            if (max_x < model.pObject[i].pVerts[vindx].x) max_x = model.pObject[i].pVerts[vindx].x;
            if (min_x > model.pObject[i].pVerts[vindx].x) min_x = model.pObject[i].pVerts[vindx].x;
            if (max_y < model.pObject[i].pVerts[vindx].y) max_y = model.pObject[i].pVerts[vindx].y;
            if (min_y > model.pObject[i].pVerts[vindx].y) min_y = model.pObject[i].pVerts[vindx].y;
            if (max_z < model.pObject[i].pVerts[vindx].z) max_z = model.pObject[i].pVerts[vindx].z;
            if (min_z > model.pObject[i].pVerts[vindx].z) min_z = model.pObject[i].pVerts[vindx].z;

            int third = gNumVertexList/3;
            gUseTextures[third] = (obj_sampler != -1) ? 1 : 0;
            gSamplerList[third] = obj_sampler;

            gNormalList[gNumVertexList] = model.pObject[i].pNormals[vindx].x;
            gNormalList[gNumVertexList+1] = model.pObject[i].pNormals[vindx].y;
            gNormalList[gNumVertexList+2] = model.pObject[i].pNormals[vindx].z;

            if (material != -1)
            {
                gColorList[gNumVertexList] = model.pMaterials[material].color[0]/255.0f;
                gColorList[gNumVertexList+1] = model.pMaterials[material].color[1]/255.0f;
                gColorList[gNumVertexList+2] = model.pMaterials[material].color[2]/255.0f;
            }
            else
            {
                gColorList[gNumVertexList] = 0.5f;
                gColorList[gNumVertexList+1] = 0.5f;
                gColorList[gNumVertexList+2] = 0.5f;
            }

            if (obj_sampler != -1)
            {
                gTexturesUVList[third*2] = model.pObject[i].pTexVerts[vindx].x;
                gTexturesUVList[third*2+1] = 1.0f-model.pObject[i].pTexVerts[vindx].y;
            }

            gNumVertexList += 3;
        }

        for (int f = 0 ; f < model.pObject[i].numOfFaces ; f++)
        {
            for (int v = 0 ; v < 3 ; v++)
            {
                gIndicesList[gNumIndicesList++] = (unsigned short)model.pObject[i].pFaces[f].vertIndex[v] + (unsigned short)last_offset;
            }
        }

        last_offset += model.pObject[i].numOfVerts;
    }

    gModelArrayInfos[gNumModelArrayInfos].numIndices = gNumIndicesList - gModelArrayInfos[gNumModelArrayInfos].indexOffset;
    gModelArrayInfos[gNumModelArrayInfos].numVertices = gNumVertexList - gModelArrayInfos[gNumModelArrayInfos].vertexOffset;

    gModelArrayInfos[gNumModelArrayInfos].max_x = max_x;
    gModelArrayInfos[gNumModelArrayInfos].min_x = min_x;
    gModelArrayInfos[gNumModelArrayInfos].max_y = max_y;
    gModelArrayInfos[gNumModelArrayInfos].min_y = min_y;
    gModelArrayInfos[gNumModelArrayInfos].max_z = max_z;
    gModelArrayInfos[gNumModelArrayInfos].min_z = min_z;

    LOGI("x -> (%f, %f)\n",min_x,max_x);
    LOGI("y -> (%f, %f)\n",min_y,max_y);
    LOGI("z -> (%f, %f)\n",min_z,max_z);

    gNumModelArrayInfos++;
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadTGA(JNIEnv * env, jobject obj, jstring filename, jbyteArray buffer)
{
    jboolean isCopy = 0;
    jbyte* data = env->GetByteArrayElements(buffer, &isCopy);
    strcpy(gTextureList[gNumTextureList].filename,env->GetStringUTFChars(filename, &isCopy));
    gTextureList[gNumTextureList].textureID = loadTGA((const char*)data);
    gNumTextureList++;
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadDDS(JNIEnv * env, jobject obj, jstring filename, jbyteArray buffer)
{
    jboolean isCopy = 0;
    jbyte* data = env->GetByteArrayElements(buffer, &isCopy);
    strcpy(gTextureList[gNumTextureList].filename,env->GetStringUTFChars(filename, &isCopy));
    gTextureList[gNumTextureList].textureID = loadDDS((const char*)data);
    gNumTextureList++;
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadBMP(JNIEnv * env, jobject obj, jstring filename, jbyteArray buffer)
{
    jboolean isCopy = 0;
    jbyte* data = env->GetByteArrayElements(buffer, &isCopy);
    strcpy(gTextureList[gNumTextureList].filename,env->GetStringUTFChars(filename, &isCopy));
    gTextureList[gNumTextureList].textureID = loadBMP((const char*)data);
    gNumTextureList++;
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_loadRAW(JNIEnv * env, jobject obj, jstring filename, jint width, jint height, jint src_size, jintArray buffer)
{
    jboolean isCopy = 0;
    jint* data = env->GetIntArrayElements(buffer, &isCopy);
    char* src = (char*)data;
    strcpy(gTextureList[gNumTextureList].filename,env->GetStringUTFChars(filename, &isCopy));
    int remainder_bytes = (width*3)%4;
    int size = (width+remainder_bytes)*height*3;
    char* dst = new char[size];
    LOGI("loadRAW %s %d %d\n",gTextureList[gNumTextureList].filename,width,height);

    int src_index = 0;
    int dst_index = 0;
    for (int y = 0 ; y < height ; y++)
    {
        for (int x = 0 ; x < width ; x++)
        {
            dst[dst_index] = src[src_index+2];
            dst[dst_index+1] = src[src_index+1];
            dst[dst_index+2] = src[src_index];
            src_index += 4;
            dst_index += 3;
        }
        dst_index += remainder_bytes;
    }

    gTextureList[gNumTextureList].data = dst;
    gLoadedBytes += src_size;
    gLoadingPercent = (gLoadedBytes*100)/gTotalBytes;
    gTextureList[gNumTextureList].width = width;
    gTextureList[gNumTextureList].height = height;
    gNumTextureList++;
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_setTotalBytes(JNIEnv * env, jobject obj, jint total)
{
    gTotalBytes = total;
    gLoadedBytes = 0;
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_doneLoadingTextures(JNIEnv * env, jobject obj)
{
    loading_state = (volatile LoadingState)BINDING_TEXTURES;
}

JNIEXPORT void JNICALL Java_com_android_gl2jni_GL2JNILib_doneLoadingModels(JNIEnv * env, jobject obj)
{
    loading_state = (volatile LoadingState)BINDING_MODELS;
}
