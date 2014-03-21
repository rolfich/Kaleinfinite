#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <iostream>
#include <vector>

#include "glew/glew.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <SDL/SDL_mixer.h>

#include "GL/glfw.h"
#include "stb/stb_image.h"
#include "imgui/imgui.h"
#include "imgui/imguiRenderGL3.h"

#include "glm/glm.hpp"
#include "glm/vec3.hpp" // glm::vec3
#include "glm/vec4.hpp" // glm::vec4, glm::ivec4
#include "glm/mat4x4.hpp" // glm::mat4
#include "glm/gtc/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale, glm::perspective
#include "glm/gtc/type_ptr.hpp" // glm::value_ptr

#define GLFW_KEY_A 65
#define GLFW_KEY_Z 90
#define GLFW_KEY_E 69
#define GLFW_KEY_R 82
#define GLFW_KEY_T 84
#define GLFW_KEY_Y 89

// Font buffers
extern const unsigned char DroidSans_ttf[];
extern const unsigned int DroidSans_ttf_len;    

// Sound
const int VOLUME = 32;

struct ShaderGLSL {
    enum ShaderType {
        VERTEX_SHADER = 1,
        FRAGMENT_SHADER = 2,
        GEOMETRY_SHADER = 4
    };
    GLuint program;
};

struct GUIStates {
    bool moveTexture;
    bool zoomTexture;
    int lockPositionX;
    int lockPositionY;
    int camera;
    double time;
    bool playing;
    static const float MOUSE_ZOOM_SPEED;
};

struct Camera {
    float radius;
    float theta;
    float phi;
    glm::vec3 o;
    glm::vec3 eye;
    glm::vec3 up;
};

struct Plane {
    glm::vec3 p1;
    glm::vec3 p2;
    glm::vec3 p3;
    glm::vec3 p4;
    glm::vec3 n;
};

struct Ray {
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 direction;
};

Mix_Music* soundInit(const char* music);
void playMusic(Mix_Music* bo);
void updatePasses(bool* passNumber, int indexTextureToWrite, int lastPass, Camera camera);
int compile_and_link_shader(ShaderGLSL & shader, int typeMask, const char * sourceBuffer, int bufferSize);
int destroy_shader(ShaderGLSL & shader);
int load_shader_from_file(ShaderGLSL & shader, const char * path, int typemask);
void camera_defaults(Camera & c);
void camera_zoom(Camera & c, float factor);
int rayMirror(Ray *r, Plane p, std::vector<glm::vec3>* vLines, bool triangle);
float triangleArea(glm::vec3 p1,glm::vec3 p2,glm::vec3 p3);
void execRaytracing(GLuint program, GLuint rayBufferFbo, GLuint rayTexture1, GLuint rayTexture2, int passIndex, int texIndex, GLuint* fboTextures, GLuint vao, int quad_triangleCount, int lastPass);
float norm(glm::vec3 v);

void init_gui_states(GUIStates & guiStates) {
    guiStates.moveTexture = false;
    guiStates.zoomTexture = false;
    guiStates.lockPositionX = 0;
    guiStates.lockPositionY = 0;
    guiStates.camera = 0;
    guiStates.time = 0.0;
    guiStates.playing = false;
}

const float GUIStates::MOUSE_ZOOM_SPEED = 0.05f;

int main( int argc, char **argv ) {
    
    int kaleiWidth = 1;
    const char* textureFile = "aa.jpg";

    //Managing the console arguments
    if(argc > 1){
        textureFile = argv[1];
    }

    std::cout << "--> Kaleidoscope width : " << kaleiWidth << std::endl;
    std::cout << "--> Texture : " << textureFile << std::endl;

    int width = 600, height= 600;
    float widthf = (float) width, heightf = (float) height;
    double t;
    float fps = 0.f;

    // Initialise GLFW
    if( !glfwInit() ) {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        exit( EXIT_FAILURE );
    }

    // Open a window and create its OpenGL context
    if( !glfwOpenWindow( width, height, 0,0,0,0, 24, 0, GLFW_WINDOW ) ) {
        fprintf( stderr, "Failed to open GLFW window\n" );

        glfwTerminate();
        exit( EXIT_FAILURE );
    }

    glfwSetWindowTitle("KALEINFINITE");

    GLenum err = glewInit();
    if (GLEW_OK != err) {
          /* Problem: glewInit failed, something is seriously wrong. */
          fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
          exit( EXIT_FAILURE );
    }

    // Ensure we can capture the escape key being pressed below
    glfwEnable( GLFW_STICKY_KEYS );

    // Enable vertical sync (on cards that support it)
    glfwSwapInterval( 1 );
    GLenum glerr = GL_NO_ERROR;
    glerr = glGetError();

    if (!imguiRenderGLInit(DroidSans_ttf, DroidSans_ttf_len)) {
        fprintf(stderr, "Could not init GUI renderer.\n");
        exit(EXIT_FAILURE);
    }

    // Init viewer structures
    Camera camera;
    camera_defaults(camera);

    GUIStates guiStates;
    init_gui_states(guiStates);

    // ################################################################################
    // ################################################################### Load shaders
    // ################################################################################
    
    // Try to load and compile shaders
    ShaderGLSL fbuffer_shader;
    const char * shaderFileFbuffer = "src/fbuffer.glsl";
    int status = load_shader_from_file(fbuffer_shader, shaderFileFbuffer, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER);
    if ( status == -1 ){
        fprintf(stderr, "Error on loading  %s\n", shaderFileFbuffer);
        exit( EXIT_FAILURE );
    }

    ShaderGLSL raytracing_shader;
    const char * shaderFileRaytracing = "src/raytracing.glsl";
    status = load_shader_from_file(raytracing_shader, shaderFileRaytracing, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER);
    if ( status == -1 ) {
        fprintf(stderr, "Error on loading  %s\n", shaderFileRaytracing);
        exit( EXIT_FAILURE );
    }

    ShaderGLSL rotate_shader;
    const char * shaderFileRotate = "src/rotate.glsl";
    status = load_shader_from_file(rotate_shader, shaderFileRotate, ShaderGLSL::VERTEX_SHADER | ShaderGLSL::FRAGMENT_SHADER);
    if ( status == -1 ) {
        fprintf(stderr, "Error on loading  %s\n", shaderFileRotate);
        exit( EXIT_FAILURE );
    }

    // ################################################################################
    // ################################################################### Load texture
    // ################################################################################

    // Load images and upload textures
    GLuint bottomTexture;
    glGenTextures(1, &bottomTexture);
    int x = 0;
    int y = 0;
    int comp = 0;

    unsigned char * bottomTexFile = stbi_load(textureFile, &x, &y, &comp, 0);
    if(bottomTexFile == NULL) {
        std::cerr << "[!] Unable to load texture" << std::endl;
        return(EXIT_FAILURE);
    }
    
    //Load into a GL texture
    GLuint mode = GL_RGB;
    if(comp==1) mode = GL_RED;
    if(comp==4) mode = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, bottomTexture);
        glTexImage2D(GL_TEXTURE_2D,0,comp,x,y,0,mode,GL_UNSIGNED_BYTE,bottomTexFile);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    fprintf(stderr, "Texture %dx%d:%d\n", x, y, comp);

    glerr = glGetError();
    if(glerr != GL_NO_ERROR)
        fprintf(stderr, "2nd OpenGL Error : %s\n", gluErrorString(glerr));

    GLuint textureBis;
    glGenTextures(1, &textureBis);

    unsigned char * textureBisFile = stbi_load("tortue.jpg", &x, &y, &comp, 0);
    if(textureBisFile == NULL) {
        std::cerr << "[!] Unable to load texture" << std::endl;
        return(EXIT_FAILURE);
    }
    
    //Load into a GL texture
    mode = GL_RGB;
    if(comp==1) mode = GL_RED;
    if(comp==4) mode = GL_RGBA;

    glBindTexture(GL_TEXTURE_2D, textureBis);
        glTexImage2D(GL_TEXTURE_2D,0,comp,x,y,0,mode,GL_UNSIGNED_BYTE,textureBisFile);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ################################################################################
    // ##################################################################### Load music
    // ################################################################################

    Mix_Music* bo = NULL;
    bo = soundInit("interloper.mp3");
    playMusic(bo);

    // ################################################################################
    // ############################################################## Create primitives
    // ################################################################################

    int prisme_triangleCount = 6;
    int prisme_triangleList[] = {0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7, 8, 9, 10, 10, 9, 11}; 
    float prisme_normals[] = {0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 
                              1, 0, -1, 1, 0, -1, 1, 0, -1, 1, 0, -1, 
                              -1, 0, -1, -1, 0, -1, -1, 0, -1, -1, 0, -1};

    float xKalei = kaleiWidth / 2.;
    float yKalei = 6.;
    float yLargeKalei = 100;
    float zKalei = sqrt(3.) * kaleiWidth / 4.;

    float prisme_vertices[] = {-xKalei-0.05, yLargeKalei, zKalei+0.05, 
                               xKalei+0.05, yLargeKalei, zKalei+0.05,
                               -xKalei-0.05, -yKalei, zKalei+0.05,
                               xKalei+0.05, -yKalei, zKalei+0.05, 

                               xKalei+0.05, yLargeKalei, zKalei+0.05, 
                               0.0, yLargeKalei, -zKalei-0.05,
                               xKalei+0.05, -yKalei, zKalei+0.05,
                               0.0, -yKalei, -zKalei-0.05,

                               0.0, yLargeKalei, -zKalei-0.05,
                               -xKalei-0.05, yLargeKalei, zKalei+0.05,
                               0.0, -yKalei, -zKalei-0.05,
                               -xKalei-0.05, -yKalei, zKalei+0.05};
    
    float xTexture = -1;
    float zTexture = -1;

    // Bottom plane
    int plane_triangleCount = 2;
    int plane_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float plane_uvs[] = {0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    float plane_vertices[] = {-1.5, -6.0, 1.5, 1.5, -6.0, 1.5, -1.5, -6.0, -1.5, 1.5, -6.0, -1.5};
    float plane_normals[] = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};

    // Final scene plane
    float plane2_vertices[] = {-4, -6.0, 4, 4, -6.0, 4, -4, -6.0, -4, 4, -6.0, -4};

    // Frame buffer quad
    int   quad_triangleCount = 2;
    int   quad_triangleList[] = {0, 1, 2, 2, 1, 3}; 
    float quad_vertices[] =  {-1.0, -1.0, 1.0, -1.0, -1.0, 1.0, 1.0, 1.0};

    // Variables
    int lastPass = 0;
    bool passNumber[7] = {false, false, false, false, false, false, false};
    int indexTextureToWrite = 0;
    int rayBufferTexture = 0;
    int changeTexture = false;

    // ################################################################################
    // ############################################################ Declare VBO and VAO
    // ################################################################################

    // Create and bind VAO and VBO
    GLuint vao[4];
    glGenVertexArrays(4,vao);

    GLuint vbo[13];
    glGenBuffers(13,vbo);

    //Prism
    glBindVertexArray(vao[0]);
    // topology
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(prisme_triangleList), prisme_triangleList, GL_STATIC_DRAW);
    // vertices
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(prisme_vertices), prisme_vertices, GL_STATIC_DRAW);
    // normals
    glBindBuffer(GL_ARRAY_BUFFER, vbo[2]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(prisme_normals), prisme_normals, GL_STATIC_DRAW);

    // Texture plane
    glBindVertexArray(vao[1]);
    // topology
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[3]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_triangleList), plane_triangleList, GL_STATIC_DRAW);
    // vertices
    glBindBuffer(GL_ARRAY_BUFFER, vbo[4]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);
    // normals
    glBindBuffer(GL_ARRAY_BUFFER, vbo[5]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_normals), plane_normals, GL_STATIC_DRAW);
    // uvs
    glBindBuffer(GL_ARRAY_BUFFER, vbo[6]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_uvs), plane_uvs, GL_STATIC_DRAW);

    //Plane 2
    glBindVertexArray(vao[2]);
    // topology
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[7]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(plane_triangleList), plane_triangleList, GL_STATIC_DRAW);
    // vertices
    glBindBuffer(GL_ARRAY_BUFFER, vbo[8]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane2_vertices), plane2_vertices, GL_STATIC_DRAW);
    // normals
    glBindBuffer(GL_ARRAY_BUFFER, vbo[9]);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_normals), plane_normals, GL_STATIC_DRAW);
    // uvs
    glBindBuffer(GL_ARRAY_BUFFER, vbo[10]);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(plane_uvs), plane_uvs, GL_STATIC_DRAW);

    // Quad
    glBindVertexArray(vao[3]);
    // Bind indices and upload data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[11]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_triangleList), quad_triangleList, GL_STATIC_DRAW);
    // Bind vertices and upload data
    glBindBuffer(GL_ARRAY_BUFFER, vbo[12]);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*2, (void*)0);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);

    // Unbind everything.
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // ################################################################################
    // ################################################################### Declare FBOs
    // ################################################################################

    // Create fbuffer frame buffer object
    GLuint fbo;
    GLuint fboTextures[3];
    GLuint fboDrawBuffers[2];
    glGenTextures(3, fboTextures);

    // Create color texture
    glBindTexture(GL_TEXTURE_2D, fboTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create normal texture
    glBindTexture(GL_TEXTURE_2D, fboTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create depth texture
    glBindTexture(GL_TEXTURE_2D, fboTextures[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create Framebuffer Object
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Attach textures to framebuffer
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, fboTextures[0], 0);
    fboDrawBuffers[0] = GL_COLOR_ATTACHMENT0;
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1 , GL_TEXTURE_2D, fboTextures[1], 0);
    fboDrawBuffers[1] = GL_COLOR_ATTACHMENT1;
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, fboTextures[2], 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "Error on building framebuffer\n");
        exit( EXIT_FAILURE );
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Create raytracing buffer
    GLuint rayBufferFbo;
    GLuint rayBufferTextures[2];
    glGenTextures(2, rayBufferTextures);
    
    // Create texture 1
    glBindTexture(GL_TEXTURE_2D, rayBufferTextures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create texture 2
    glBindTexture(GL_TEXTURE_2D, rayBufferTextures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create second Framebuffer 
    glGenFramebuffers(1, &rayBufferFbo);

    // #####################################################################################################################
    // ############################################################################################################ Renderer
    // #####################################################################################################################

    do
    {
        t = glfwGetTime();

        // Mouse states
        int leftButton = glfwGetMouseButton( GLFW_MOUSE_BUTTON_LEFT );
        int rightButton = glfwGetMouseButton( GLFW_MOUSE_BUTTON_RIGHT );
        int middleButton = glfwGetMouseButton( GLFW_MOUSE_BUTTON_MIDDLE );

        if( leftButton == GLFW_PRESS )
            guiStates.moveTexture = true;
        else
            guiStates.moveTexture = false;

        if( rightButton == GLFW_PRESS )
            guiStates.zoomTexture = true;
        else
            guiStates.zoomTexture = false;

        // Camera movements
        int altPressed = glfwGetKey(GLFW_KEY_LSHIFT);
        
        if (!altPressed && (leftButton == GLFW_PRESS || rightButton == GLFW_PRESS || middleButton == GLFW_PRESS)) {
            int x; int y;
            glfwGetMousePos(&x, &y);
            guiStates.lockPositionX = x;
            guiStates.lockPositionY = y;
        }

        if (altPressed == GLFW_PRESS) {
            int mousex; int mousey;
            glfwGetMousePos(&mousex, &mousey);
            int diffLockPositionX = mousex - guiStates.lockPositionX;
            int diffLockPositionY = mousey - guiStates.lockPositionY;
            
            if (guiStates.zoomTexture) {
                float zoomDir = 0.0;
                if (diffLockPositionX > 0) {
                    zoomDir = -1.f;
                }
                else if (diffLockPositionX < 0 ) {
                    zoomDir = 1.f;
                }
                camera_zoom(camera, zoomDir * GUIStates::MOUSE_ZOOM_SPEED);
            }
        
            else if (guiStates.moveTexture) {

                zTexture = diffLockPositionY/100.;
                xTexture = diffLockPositionX/100.;

                plane_vertices[0] += xTexture;
                plane_vertices[2] += zTexture;
                plane_vertices[3] += xTexture;
                plane_vertices[5] += zTexture;
                plane_vertices[6] += xTexture;
                plane_vertices[8] += zTexture;
                plane_vertices[9] += xTexture;
                plane_vertices[11] += zTexture;

                glBindVertexArray(vao[1]);
                glBindBuffer(GL_ARRAY_BUFFER, vbo[4]);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
                glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);
            }
            guiStates.lockPositionX = mousex;
            guiStates.lockPositionY = mousey;
        }

        // Variables for the demo

        int aPressed = glfwGetKey(GLFW_KEY_A);
        int rPressed = glfwGetKey(GLFW_KEY_R);
        int tPressed = glfwGetKey(GLFW_KEY_T);
        int yPressed = glfwGetKey(GLFW_KEY_Y);

        if(rPressed == GLFW_PRESS) {
            camera_zoom(camera, GUIStates::MOUSE_ZOOM_SPEED/6.);
        }else if(tPressed == GLFW_PRESS){
            camera_zoom(camera, -GUIStates::MOUSE_ZOOM_SPEED/6.);
        }

        if(yPressed == GLFW_PRESS) {
            if(changeTexture) changeTexture = false;
            else changeTexture = true;
        }

        // Reset the textured quad coordinates
        if(aPressed == GLFW_PRESS) {

            plane_vertices[0] = -1.5;
            plane_vertices[2] = 1.5;
            plane_vertices[3] = 1.5;
            plane_vertices[5] = 1.5;
            plane_vertices[6] = -1.5;
            plane_vertices[8] = -1.5;
            plane_vertices[9] = 1.5;
            plane_vertices[11] = -1.5;

            glBindVertexArray(vao[1]);
            glBindBuffer(GL_ARRAY_BUFFER, vbo[4]);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
            glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);
        }

        updatePasses(passNumber, indexTextureToWrite, lastPass, camera);

        // Get matrices
        glm::mat4 projection = glm::perspective(45.0f, widthf / heightf, 0.1f, 100.f); 
        glm::mat4 worldToView = glm::lookAt(camera.eye, camera.o, camera.up);
        glm::mat4 worldToScreen = projection * worldToView;
        glm::mat4 screenToWorld = glm::transpose(glm::inverse(worldToScreen));
        glm::mat4 objectToWorld;
        glm::mat3 rotation = glm::mat3(glm::vec3(cos(t/5), 0.0, -sin(t/5)), glm::vec3( 0.0, 1.0, 0.0), glm::vec3(sin(t/5), 0.0, cos(t/5)));

        // ##################################################################################################################
        // ################################################################################################### Draw the scene
        // ##################################################################################################################
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glDrawBuffers(2, fboDrawBuffers);

        // Viewport 
        glViewport( 0, 0, width, height  );

        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Default states
        glEnable(GL_DEPTH_TEST);

        // Bind shader
        glUseProgram(fbuffer_shader.program);

        // Upload uniforms
        glUniformMatrix4fv(glGetUniformLocation(fbuffer_shader.program, "Projection"), 1, 0, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(fbuffer_shader.program, "View"), 1, 0, glm::value_ptr(worldToView));
        glUniform1f(glGetUniformLocation(fbuffer_shader.program, "Time"), t/2.);
    
        // Send textures locations
        glUniform1i(glGetUniformLocation(fbuffer_shader.program, "Texture"), 0);

        glBindVertexArray(vao[0]);
        glDrawElements(GL_TRIANGLES, prisme_triangleCount*3, GL_UNSIGNED_INT, (void*)0);

        // Activate texture units and bind textures to them
        glActiveTexture(GL_TEXTURE0);
        
        // Change texture
        if(changeTexture) {
            glBindTexture(GL_TEXTURE_2D, textureBis);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, bottomTexture);
        }

        glBindVertexArray(vao[1]);
        glDrawElements(GL_TRIANGLES, plane_triangleCount*3, GL_UNSIGNED_INT, (void*)0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Unbind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // ####################################### Draw the first pass

        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport( 0, 0, width, height );

        glUseProgram(raytracing_shader.program);
        glUniformMatrix4fv(glGetUniformLocation(raytracing_shader.program, "InverseViewProjection"), 1, 0, glm::value_ptr(screenToWorld));
        glUniformMatrix4fv(glGetUniformLocation(raytracing_shader.program, "Projection"), 1, 0, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(raytracing_shader.program, "View"), 1, 0, glm::value_ptr(worldToView));
        
        glUniform1f(glGetUniformLocation(raytracing_shader.program, "XKalei"), xKalei);
        glUniform1f(glGetUniformLocation(raytracing_shader.program, "YKalei"), yKalei);
        glUniform1f(glGetUniformLocation(raytracing_shader.program, "YLargeKalei"), yLargeKalei);
        glUniform1f(glGetUniformLocation(raytracing_shader.program, "ZKalei"), zKalei);

        glUniform3fv(glGetUniformLocation(raytracing_shader.program, "CameraPosition"), 1, glm::value_ptr(camera.eye));

        execRaytracing(raytracing_shader.program, rayBufferFbo, rayBufferTextures[0], rayBufferTextures[0], 0, 0, fboTextures, vao[3], quad_triangleCount, lastPass);

        // ####################################### Draw the second pass
        if(passNumber[0]){
            // Clear the front buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport( 0, 0, width, height );

            execRaytracing(raytracing_shader.program, rayBufferFbo, rayBufferTextures[1], rayBufferTextures[0], 1, 3, fboTextures, vao[3], quad_triangleCount, lastPass);    
        }
        

        // ####################################### Draw the third pass
        if(passNumber[1]){
            // Clear the front buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport( 0, 0, width, height );

            execRaytracing(raytracing_shader.program, rayBufferFbo, rayBufferTextures[0], rayBufferTextures[1], 2, 3, fboTextures, vao[3], quad_triangleCount, lastPass);
        }

        // ####################################### Draw the fourth pass
        if(passNumber[2]){
            // Clear the front buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport( 0, 0, width, height );

            execRaytracing(raytracing_shader.program, rayBufferFbo, rayBufferTextures[1], rayBufferTextures[0], 3, 3, fboTextures, vao[3], quad_triangleCount, lastPass);
        }

        // ####################################### Draw the fifth pass
        if(passNumber[3]){
            // Clear the front buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport( 0, 0, width, height );

            execRaytracing(raytracing_shader.program, rayBufferFbo, rayBufferTextures[0], rayBufferTextures[1], 4, 3, fboTextures, vao[3], quad_triangleCount, lastPass);
        }

        // ####################################### Draw the sixth pass
        if(passNumber[4]){
            // Clear the front buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport( 0, 0, width, height );

            execRaytracing(raytracing_shader.program, rayBufferFbo, rayBufferTextures[1], rayBufferTextures[0], 5, 3, fboTextures, vao[3], quad_triangleCount, lastPass);
        }

        // ####################################### Draw the seventh pass
        if(passNumber[5]){
            // Clear the front buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glViewport( 0, 0, width, height );

            execRaytracing(raytracing_shader.program, rayBufferFbo, rayBufferTextures[0], rayBufferTextures[1], 6, 3, fboTextures, vao[3], quad_triangleCount, lastPass);
        }

        // ####################################### Rotate the scene
        // Clear the front buffer
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glViewport( 0, 0, width, height );

        glUseProgram(rotate_shader.program);
        glUniformMatrix4fv(glGetUniformLocation(rotate_shader.program, "Projection"), 1, 0, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(rotate_shader.program, "View"), 1, 0, glm::value_ptr(worldToView));
        glUniformMatrix3fv(glGetUniformLocation(rotate_shader.program, "Rotation"), 1, 0, glm::value_ptr(rotation));
        glUniform3fv(glGetUniformLocation(rotate_shader.program, "CameraPosition"), 1, glm::value_ptr(camera.eye));
        glUniform1i(glGetUniformLocation(rotate_shader.program, "FBOTexture"), 0);

        if(indexTextureToWrite == 0) {
            rayBufferTexture = rayBufferTextures[1];
        }else rayBufferTexture = rayBufferTextures[0];

        // Bind previous raytracing texture to unit 0
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rayBufferTexture);    

        // Blit above the rest
        glDisable(GL_DEPTH_TEST);
        // Render vao

        glBindVertexArray(vao[2]);
        glDrawElements(GL_TRIANGLES, plane_triangleCount*3, GL_UNSIGNED_INT, (void*)0);

        // vertices
        glBindBuffer(GL_ARRAY_BUFFER, vbo[8]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        glBufferData(GL_ARRAY_BUFFER, sizeof(plane2_vertices), plane2_vertices, GL_STATIC_DRAW);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, 0);

        // Unbind framebuffer
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Texture animation
        zTexture = cos(t)/300;
        xTexture = sin(t)/300;

        for(int i = 0; i< 10; i+=3){
            plane_vertices[i] += xTexture;        
            plane_vertices[i+2] += zTexture;    
        }

        glBindVertexArray(vao[1]);
        glBindBuffer(GL_ARRAY_BUFFER, vbo[4]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GL_FLOAT)*3, (void*)0);
        glBufferData(GL_ARRAY_BUFFER, sizeof(plane_vertices), plane_vertices, GL_STATIC_DRAW);

        // Draw UI
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glViewport(0, 0, width, height);

        unsigned char mbut = 0;
        int mscroll = 0;
        int mousex; int mousey;
        glfwGetMousePos(&mousex, &mousey);
        mousey = height - mousey;

        if( leftButton == GLFW_PRESS )
            mbut |= IMGUI_MBUT_LEFT;

        imguiBeginFrame(mousex, mousey, mbut, mscroll);
        int logScroll = 0;
        char lineBuffer[512];
        imguiBeginScrollArea("", width - 100, height - 40, 100, 60, &logScroll);
        sprintf(lineBuffer, "FPS %f", fps);
        imguiLabel(lineBuffer);

        imguiEndScrollArea();
        imguiEndFrame();
        imguiRenderGLDraw(width, height);

        glDisable(GL_BLEND);

        // Check for errors
        GLenum err = glGetError();
        if(err != GL_NO_ERROR) {
            fprintf(stderr, "OpenGL Error : %s\n", gluErrorString(err));
        }

        // Swap buffers
        glfwSwapBuffers();

        double newTime = glfwGetTime();
        fps = 1.f/ (newTime - t);
    }
    
    // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey( GLFW_KEY_ESC ) != GLFW_PRESS &&
           glfwGetWindowParam( GLFW_OPENED ) );

    // Clean UI
    imguiRenderGLDestroy();

    // Close OpenGL window and terminate GLFW
    glDeleteTextures(1, &bottomTexture);
    glDeleteBuffers(10, vbo);
    glDeleteBuffers(1, &fbo);
    glDeleteTextures(3, fboTextures);
    glDeleteBuffers(1, &rayBufferFbo);
    glDeleteTextures(2, rayBufferTextures);
    glDeleteVertexArrays(4, vao);
    Mix_CloseAudio();
    Mix_FreeMusic(bo);
    glfwTerminate();
    exit( EXIT_SUCCESS );
}

void execRaytracing(GLuint program, GLuint rayBufferFbo, GLuint rayTexture1, GLuint rayTexture2, int passIndex, int texIndex, GLuint* fboTextures, GLuint vao, int quad_triangleCount, int lastPass){

    glBindFramebuffer(GL_FRAMEBUFFER, rayBufferFbo); 
    // On attache la texture 0 pour dessiner dedans
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 , GL_TEXTURE_2D, rayTexture1, 0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        fprintf(stderr, "Error on building framebuffer\n");
        exit( EXIT_FAILURE );
    }

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "PassIndex"), passIndex);
    glUniform1i(glGetUniformLocation(program, "FBOTexture"), texIndex);

    // Bind color to unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fboTextures[0]);        
    // Bind normal to unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fboTextures[1]);    
    // Bind depth to unit 2
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, fboTextures[2]);        

    if(passIndex > 0) {
        // Bind previous raytracing texture to unit 3
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, rayTexture2);    
    }

    // Blit above the rest
    glDisable(GL_DEPTH_TEST);
    // Render vao
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, quad_triangleCount * 3, GL_UNSIGNED_INT, (void*)0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Unbind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

Mix_Music* soundInit(const char* music){

    if(Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, MIX_DEFAULT_CHANNELS, 1024) == -1){
        std::cerr << "[!]-> Unable to initialize sound player" << std::endl;
        exit(-1);
    }

    Mix_Music* bo = NULL;    
    
    //load music
    bo = Mix_LoadMUS(music);
    if(bo == NULL){
        std::cout<<"[!]-> Unable to load music : "<< Mix_GetError() <<std::endl;
    }
    
    Mix_VolumeMusic(VOLUME);
    return bo;
}

void playMusic(Mix_Music* bo){
    Mix_PlayMusic(bo, -1);
}

void updatePasses(bool* passNumber, int indexTextureToWrite, int lastPass, Camera camera){

    if(camera.radius < 1){
        passNumber[0] = true; 
        passNumber[1] = true;
        passNumber[2] = false;
        passNumber[3] = false;
        passNumber[4] = false;
        passNumber[5] = false;
        indexTextureToWrite = 1;
        lastPass = 2;
    } else if(camera.radius >= 1 && camera.radius < 2){
        passNumber[0] = true; 
        passNumber[1] = true;
        passNumber[2] = true;
        passNumber[3] = false;
        passNumber[4] = false;
        passNumber[5] = false;
        indexTextureToWrite = 0;
        lastPass = 3;
    } else if(camera.radius >= 2  && camera.radius < 8){
        passNumber[0] = true; 
        passNumber[1] = true;
        passNumber[2] = true;
        passNumber[3] = true;
        passNumber[4] = false;
        passNumber[5] = false;
        indexTextureToWrite = 1;
        lastPass = 4;
    } else if(camera.radius >= 8  && camera.radius < 25){
        passNumber[0] = true; 
        passNumber[1] = true;
        passNumber[2] = true;
        passNumber[3] = true;
        passNumber[4] = true;
        passNumber[5] = false;
        indexTextureToWrite = 0;
        lastPass = 5;
    } else if(camera.radius >= 25){
        passNumber[0] = true; 
        passNumber[1] = true;
        passNumber[2] = true;
        passNumber[3] = true;
        passNumber[4] = true;
        passNumber[5] = true;
        indexTextureToWrite = 1;
        lastPass = 6;
    }
}

int  compile_and_link_shader(ShaderGLSL & shader, int typeMask, const char * sourceBuffer, int bufferSize)
{
    // Create program object
    shader.program = glCreateProgram();
    
    //Handle Vertex Shader
    GLuint vertexShaderObject ;
    if (typeMask & ShaderGLSL::VERTEX_SHADER)
    {
        // Create shader object for vertex shader
        vertexShaderObject = glCreateShader(GL_VERTEX_SHADER);
        // Add #define VERTEX to buffer
        const char * sc[3] = { "#version 150\n", "#define VERTEX\n", sourceBuffer};
        glShaderSource(vertexShaderObject, 
                       3, 
                       sc,
                       NULL);
        // Compile shader
        glCompileShader(vertexShaderObject);

        // Get error log size and print it eventually
        int logLength;
        glGetShaderiv(vertexShaderObject, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 1)
        {
            char * log = new char[logLength];
            glGetShaderInfoLog(vertexShaderObject, logLength, &logLength, log);
            fprintf(stderr, "Error in compiling vertex shader : %s", log);
            fprintf(stderr, "%s\n%s\n%s", sc[0], sc[1], sc[2]);
            delete[] log;
        }
        // If an error happend quit
        int status;
        glGetShaderiv(vertexShaderObject, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE)
            return -1;          

        //Attach shader to program
        glAttachShader(shader.program, vertexShaderObject);
    }

    // Handle Geometry shader
    GLuint geometryShaderObject ;
    if (typeMask & ShaderGLSL::GEOMETRY_SHADER)
    {
        // Create shader object for Geometry shader
        geometryShaderObject = glCreateShader(GL_GEOMETRY_SHADER);
        // Add #define Geometry to buffer
        const char * sc[3] = { "#version 150\n", "#define GEOMETRY\n", sourceBuffer};
        glShaderSource(geometryShaderObject, 
                       3, 
                       sc,
                       NULL);
        // Compile shader
        glCompileShader(geometryShaderObject);

        // Get error log size and print it eventually
        int logLength;
        glGetShaderiv(geometryShaderObject, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 1)
        {
            char * log = new char[logLength];
            glGetShaderInfoLog(geometryShaderObject, logLength, &logLength, log);
            fprintf(stderr, "Error in compiling Geometry shader : %s \n", log);
            fprintf(stderr, "%s\n%s\n%s", sc[0], sc[1], sc[2]);
            delete[] log;
        }
        // If an error happend quit
        int status;
        glGetShaderiv(geometryShaderObject, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE)
            return -1;          

        //Attach shader to program
        glAttachShader(shader.program, geometryShaderObject);
    }


    // Handle Fragment shader
    GLuint fragmentShaderObject ;
    if (typeMask && ShaderGLSL::FRAGMENT_SHADER)
    {
        // Create shader object for fragment shader
        fragmentShaderObject = glCreateShader(GL_FRAGMENT_SHADER);
        // Add #define fragment to buffer
        const char * sc[3] = { "#version 150\n", "#define FRAGMENT\n", sourceBuffer};
        glShaderSource(fragmentShaderObject, 
                       3, 
                       sc,
                       NULL);
        // Compile shader
        glCompileShader(fragmentShaderObject);

        // Get error log size and print it eventually
        int logLength;
        glGetShaderiv(fragmentShaderObject, GL_INFO_LOG_LENGTH, &logLength);
        if (logLength > 1)
        {
            char * log = new char[logLength];
            glGetShaderInfoLog(fragmentShaderObject, logLength, &logLength, log);
            fprintf(stderr, "Error in compiling fragment shader : %s \n", log);
            fprintf(stderr, "%s\n%s\n%s", sc[0], sc[1], sc[2]);
            delete[] log;
        }
        // If an error happend quit
        int status;
        glGetShaderiv(fragmentShaderObject, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE)
            return -1;          

        //Attach shader to program
        glAttachShader(shader.program, fragmentShaderObject);
    }

    // Bind attribute location
    glBindAttribLocation(shader.program,  0,  "VertexPosition");
    glBindAttribLocation(shader.program,  1,  "VertexNormal");
    glBindAttribLocation(shader.program,  2,  "VertexTexCoord");
    glBindFragDataLocation(shader.program, 0, "Color");
    glBindFragDataLocation(shader.program, 1, "Normal");

    // Link attached shaders
    glLinkProgram(shader.program);

    // Clean
    if (typeMask & ShaderGLSL::VERTEX_SHADER) {
        glDeleteShader(vertexShaderObject);
    }
    if (typeMask && ShaderGLSL::GEOMETRY_SHADER) {
        glDeleteShader(fragmentShaderObject);
    }
    if (typeMask && ShaderGLSL::FRAGMENT_SHADER) {
        glDeleteShader(fragmentShaderObject);
    }

    // Get link error log size and print it eventually
    int logLength;
    glGetProgramiv(shader.program, GL_INFO_LOG_LENGTH, &logLength);
    if (logLength > 1) {
        char * log = new char[logLength];
        glGetProgramInfoLog(shader.program, logLength, &logLength, log);
        fprintf(stderr, "Error in linking shaders : %s \n", log);
        delete[] log;
    }
    int status;
    glGetProgramiv(shader.program, GL_LINK_STATUS, &status);        
    if (status == GL_FALSE)
        return -1;

    return 0;
}

int  destroy_shader(ShaderGLSL & shader) {
    glDeleteProgram(shader.program);
    shader.program = 0;
    return 0;
}

int load_shader_from_file(ShaderGLSL & shader, const char * path, int typemask) {
    int status;
    FILE * shaderFileDesc = fopen( path, "rb" );
    if (!shaderFileDesc)
        return -1;
    fseek ( shaderFileDesc , 0 , SEEK_END );
    long fileSize = ftell ( shaderFileDesc );
    rewind ( shaderFileDesc );
    char * buffer = new char[fileSize + 1];
    fread( buffer, 1, fileSize, shaderFileDesc );
    buffer[fileSize] = '\0';
    status = compile_and_link_shader( shader, typemask, buffer, fileSize );
    delete[] buffer;
    return status;
}

void camera_compute(Camera & c) {
    c.eye.x = cos(c.theta) * sin(c.phi) * c.radius + c.o.x;   
    c.eye.y = cos(c.phi) * c.radius + c.o.y ;
    c.eye.z = sin(c.theta) * sin(c.phi) * c.radius + c.o.z;   
    c.up = glm::vec3(0.f, c.phi < M_PI ?1.f:-1.f, 0.f);
}

void camera_defaults(Camera & c) {
    c.phi = 2.f*3.14; // top
    c.theta = 3.14/2.f; // front
    c.radius = 2.f; // height
    camera_compute(c);
}

void camera_zoom(Camera & c, float factor) {
    c.radius += factor * c.radius ;
    if(c.radius >= 80.) c.radius = 80.;
    if (c.radius < 0.2) {
        c.radius = 0.2f;
        c.o = c.eye + glm::normalize(c.o - c.eye) * c.radius;
    }
    camera_compute(c);
}