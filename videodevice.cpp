#include "precomp.h"
#include "videodevice.h"
#include "result.h"
#include "numeric.h"
#include "logger.h"


#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// platform specific includes
#ifdef UNIX
#include <GL/glx.h>
#define vdGetProcAddress glXGetProcAddress
#define PROCADDRNAMEPTR const GLubyte*
#elif defined(WIN32)
#include <gl/wglext.h>
#define vdGetProcAddress wglGetProcAddress
#define PROCADDRNAMEPTR LPCSTR
#endif

#ifdef __AVX__
#undef __AVX__
#endif

#include <algorithm>

namespace {
    PFNGLCREATESHADERPROC glCreateShader;
    PFNGLGETPROGRAMIVPROC glGetProgramiv;
    PFNGLSHADERSOURCEPROC glShaderSource;
    PFNGLCOMPILESHADERPROC glCompileShader;
    PFNGLGETSHADERIVPROC glGetShaderiv;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
    PFNGLCREATEPROGRAMPROC glCreateProgram;
    PFNGLATTACHSHADERPROC glAttachShader;
    PFNGLLINKPROGRAMPROC glLinkProgram;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
    PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
    PFNGLUSEPROGRAMPROC glUseProgram;
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
    PFNGLGENBUFFERSPROC glGenBuffers;
    PFNGLBINDBUFFERPROC glBindBuffer;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
    PFNGLUNIFORM1IPROC glUniform1i;
    PFNGLUNIFORM4FPROC glUniform4f;
    PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;
    PFNGLBUFFERDATAPROC glBufferData;
    PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
    PFNGLDELETEBUFFERSPROC glDeleteBuffers;
    PFNGLACTIVETEXTUREPROC glActiveTexture;

    videodevice::Device* currentDevice = NULL;

    void CheckOpenGLError(const char* stmt, const char* fname, int line)
    {
        bool error = false;
        GLenum err;
        while( (err = glGetError()) != GL_NO_ERROR)
        {
            logger::Error("OpenGL error %08x, %s at %s:%i - for %s\n", err, gluErrorString(err), fname, line, stmt);
            error = true;
        }

        if(error)
        {
            #ifdef _DEBUG
            assert(!error);
            #else
            abort();
            #endif
        }
    }

    #define GL_CHECK(stmt)\
        do { \
            stmt; \
            CheckOpenGLError(#stmt, __FILE__, __LINE__); \
        } while (0)


    // GetProcAddress
    typedef void(*Proc)();

    Proc GetProcAddress(PROCADDRNAMEPTR n )
    {
        Proc addr = (Proc) vdGetProcAddress(n);
        if(!addr) 
        {
            logger::Error("GetProcAddress %s not found!", reinterpret_cast<const char*>(n));
        }
        assert(addr);
        return addr;
    }


    void InitGLext()
    {
        glCreateShader = (PFNGLCREATESHADERPROC) GetProcAddress((PROCADDRNAMEPTR) "glCreateShader");
        glGetProgramiv = (PFNGLGETPROGRAMIVPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetProgramiv");
        glShaderSource = (PFNGLSHADERSOURCEPROC) GetProcAddress((PROCADDRNAMEPTR) "glShaderSource");
        glCompileShader = (PFNGLCOMPILESHADERPROC) GetProcAddress((PROCADDRNAMEPTR) "glCompileShader");
        glGetShaderiv = (PFNGLGETSHADERIVPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetShaderiv");
        glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetShaderInfoLog");
        glCreateProgram = (PFNGLCREATEPROGRAMPROC) GetProcAddress((PROCADDRNAMEPTR) "glCreateProgram");
        glAttachShader = (PFNGLATTACHSHADERPROC) GetProcAddress((PROCADDRNAMEPTR) "glAttachShader");
        glLinkProgram = (PFNGLLINKPROGRAMPROC) GetProcAddress((PROCADDRNAMEPTR) "glLinkProgram");
        glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetUniformLocation");
        glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC) GetProcAddress((PROCADDRNAMEPTR) "glGetAttribLocation");
        glUseProgram = (PFNGLUSEPROGRAMPROC) GetProcAddress((PROCADDRNAMEPTR) "glUseProgram");
        glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC) GetProcAddress((PROCADDRNAMEPTR) "glGenVertexArrays");
        glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC) GetProcAddress((PROCADDRNAMEPTR) "glBindVertexArray");
        glGenBuffers = (PFNGLGENBUFFERSPROC) GetProcAddress((PROCADDRNAMEPTR) "glGenBuffers");
        glBindBuffer = (PFNGLBINDBUFFERPROC) GetProcAddress((PROCADDRNAMEPTR) "glBindBuffer");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC) GetProcAddress((PROCADDRNAMEPTR) "glVertexAttribPointer");
        glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC) GetProcAddress((PROCADDRNAMEPTR) "glEnableVertexAttribArray");
        glUniform1i = (PFNGLUNIFORM1IPROC) GetProcAddress((PROCADDRNAMEPTR) "glUniform1i");
        glUniform4f = (PFNGLUNIFORM4FPROC) GetProcAddress((PROCADDRNAMEPTR) "glUniform4f");
        glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC) GetProcAddress((PROCADDRNAMEPTR) "glUniformMatrix4fv");
        glBufferData = (PFNGLBUFFERDATAPROC) GetProcAddress((PROCADDRNAMEPTR) "glBufferData");
        glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC) GetProcAddress((PROCADDRNAMEPTR) "glDeleteVertexArrays");
        glDeleteBuffers = (PFNGLDELETEBUFFERSPROC) GetProcAddress((PROCADDRNAMEPTR) "glDeleteBuffers");
        glActiveTexture = (PFNGLACTIVETEXTUREPROC)GetProcAddress((PROCADDRNAMEPTR) "glActiveTexture");
    }

    Result BuildShader(std::string const &shaderSource, GLuint &shader, GLenum type) {
        Result result;
        int32_t size = static_cast<int32_t>(shaderSource.length());
        GL_CHECK(shader = glCreateShader(type));
        char const *cShaderSource = shaderSource.c_str();
        GL_CHECK(glShaderSource(shader, 1, (GLchar const **)&cShaderSource, &size));
        GL_CHECK(glCompileShader(shader));
        GLint status = 0;;
        GL_CHECK(glGetShaderiv(shader, GL_COMPILE_STATUS, &status));
        if (status != GL_TRUE) {
            int32_t length = 0;
            GL_CHECK(glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length));

            char *log = new char[length];
            GL_CHECK(glGetShaderInfoLog(shader, length, &length, log));

            result = Result(false, "Device failed to compiled shader %s", log);

            delete[] log;
        }
        
        return result;
    }

    Result BuildProgram(const std::string vs, const std::string& fs, GLuint& program)
    {
        Result result;

        GLuint vertexShader, fragmentShader;
        result = BuildShader(vs, vertexShader, GL_VERTEX_SHADER);
        if(!result)
        {
            return result;
        }
        
        result = BuildShader(fs, fragmentShader, GL_FRAGMENT_SHADER);
        if(!result)
        {
            return result;
        }
        
        GL_CHECK(program = glCreateProgram());
        GL_CHECK(glAttachShader(program, vertexShader));
        GL_CHECK(glAttachShader(program, fragmentShader));
        GL_CHECK(glLinkProgram(program));
        GLint status;
        GL_CHECK(glGetProgramiv(program, GL_LINK_STATUS, &status));
        if (status != GL_TRUE) {
            int32_t length;
            GL_CHECK(glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length));
            
            char *log = new char[length];
            GL_CHECK(glGetShaderInfoLog(program, length, &length, log));

            result = Result(false, "DeviceBuildProgram failed to link program %s", log);
            delete[] log;

            return result;
        }
        
        return result;
    }

    void GetImageCoord(uint32_t windowWidth, uint32_t windowHeight, uint32_t textureWidth, uint32_t textureHeight, float& x1, float& x2, float& y1, float& y2)
    {
        const float ww = static_cast<float>(windowWidth);
        const float wh = static_cast<float>(windowHeight);
        const float tw = static_cast<float>(textureWidth);
        const float th = static_cast<float>(textureHeight);
        const float tr = tw / th;

        const float maxw = std::max(ww,tw);
        const float maxh = std::max(wh,th);

        float adjustWidth = ww;
        float adjustHeight = wh;
        float adjustRatio = 0.0f;

        if( maxw > maxh )
        {
            float w = maxw;
            float h = w / tr;

            if( h > maxh )
            {
                h = maxh;
                w = h * tr;
            }

            adjustWidth = w;
            adjustHeight = h;
        }
        else
        {
            float h = maxh;
            float w = h * tr;

            if( w > maxw )
            {
                w = maxw;
                h = h * tr;
            }

            adjustWidth = w;
            adjustHeight = h;
        }

        adjustRatio = adjustWidth / adjustHeight;

        if( !numeric::equals(tr, adjustRatio) )
        {
            logger::Debug("ratio differ");
            if( adjustRatio < 1.0f )
            {
                adjustWidth = ww;
                adjustHeight = adjustWidth / tr;
            }
        }

        x1 = ww / 2.0f - adjustWidth / 2.0f;
        x2 = x1 + adjustWidth;
        y1 = wh / 2.0f - adjustHeight / 2.0f;
        y2 = y1 + adjustHeight;

        logger::Debug("w %f h %f adjustWidth %f adjustHeight %f tr %f ar %f x1 %f x2 %f y1 %f y2 %f", ww, wh, adjustWidth, adjustHeight, tr, adjustWidth / adjustHeight, x1, x2, y1, y2);
    }

    class Rgb24Renderer : public videodevice::Renderer
    {
    public:
        Rgb24Renderer()
        : textureWidth(0),
          textureHeight(0),
          windowWidth(0),
          windowHeight(0),
          vertexArray(0),
          vertexBuffer(0),
          elementBuffer(0),
          frameTexture(0),
          program(0)
        {
            memset(attribs, 0, sizeof(attribs));
            memset(uniforms, 0, sizeof(uniforms));
        }

        virtual ~Rgb24Renderer()
        {
            GL_CHECK(glDeleteVertexArrays(1, &vertexArray));
            GL_CHECK(glDeleteBuffers(1, &vertexBuffer));
            GL_CHECK(glDeleteBuffers(1, &elementBuffer));
            GL_CHECK(glDeleteTextures(1, &frameTexture));
        }
        
        virtual Result Create()
        {
            Result result;

            logger::Info("Creating RGB24 Renderer");

            const std::string vertexShaderSource = 
            "#version 150\n"
            "in vec3 vertex;\n"
            "in vec2 texCoord0;\n"
            "uniform mat4 mvpMatrix;\n"
            "out vec2 texCoord;\n"
            "void main() {\n"
            "   gl_Position = mvpMatrix * vec4(vertex, 1.0);\n"
            "   texCoord = texCoord0;\n"
            "}\n";

            const std::string fragmentShaderSource =
            "#version 150\n"
            "uniform sampler2D frameTex;\n"
            "in vec2 texCoord;\n"
            "out vec4 fragColor;\n"
            "vec4 cubic(float v){\n"
            "    vec4 n = vec4(1.0, 2.0, 3.0, 4.0) - v;\n"
            "    vec4 s = n * n * n;\n"
            "    float x = s.x;\n"
            "    float y = s.y - 4.0 * s.x;\n"
            "    float z = s.z - 4.0 * s.y + 6.0 * s.x;\n"
            "    float w = 6.0 - x - y - z;\n"
            "    return vec4(x, y, z, w) * (1.0/6.0);\n"
            "}\n"
            "vec4 textureBicubic(sampler2D sampler, vec2 texCoords){\n"
            "\n"
            "   vec2 texSize = textureSize(sampler, 0);\n"
            "   vec2 invTexSize = 1.0 / texSize;\n"
            "\n"
            "   texCoords = texCoords * texSize - 0.5;\n"
            "\n"
            "\n"
            "    vec2 fxy = fract(texCoords);\n"
            "    texCoords -= fxy;\n"
            "\n"
            "    vec4 xcubic = cubic(fxy.x);\n"
            "    vec4 ycubic = cubic(fxy.y);\n"
            "\n"
            "    vec4 c = texCoords.xxyy + vec2 (-0.5, +1.5).xyxy;\n"
            "\n"
            "    vec4 s = vec4(xcubic.xz + xcubic.yw, ycubic.xz + ycubic.yw);\n"
            "    vec4 offset = c + vec4 (xcubic.yw, ycubic.yw) / s;\n"
            "\n"
            "    offset *= invTexSize.xxyy;\n"
            "\n"
            "    vec4 sample0 = texture(sampler, offset.xz);\n"
            "    vec4 sample1 = texture(sampler, offset.yz);\n"
            "    vec4 sample2 = texture(sampler, offset.xw);\n"
            "    vec4 sample3 = texture(sampler, offset.yw);\n"
            "\n"
            "    float sx = s.x / (s.x + s.y);\n"
            "    float sy = s.z / (s.z + s.w);\n"
            "\n"
            "    return mix(\n"
            "       mix(sample3, sample2, sx), mix(sample1, sample0, sx)\n"
            "    , sy);\n"
            "}\n"
            "void main() {\n"
            "   fragColor = textureBicubic(frameTex, texCoord);\n"
            "}\n";

            result = BuildProgram(vertexShaderSource, fragmentShaderSource, program);
            if(!result)
            {
                return result;
            }

            GL_CHECK(uniforms[MVP_MATRIX] = glGetUniformLocation(program, "mvpMatrix"));
            GL_CHECK(uniforms[FRAME_TEX] = glGetUniformLocation(program, "frameTex"));
            
            GL_CHECK(attribs[VERTICES] = glGetAttribLocation(program, "vertex"));
            GL_CHECK(attribs[TEX_COORDS] = glGetAttribLocation(program, "texCoord0"));

            GL_CHECK(glUseProgram(program));
            GL_CHECK(glGenVertexArrays(1, &vertexArray));
            GL_CHECK(glGenBuffers(1, &vertexBuffer));
            GL_CHECK(glGenBuffers(1, &elementBuffer));
            GL_CHECK(glGenTextures(1, &frameTexture));

            return result;
        }

        virtual Result Draw(videodevice::FrameBuffer* f)
        {
            Result result;
            GL_CHECK(glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, textureWidth, 
                            textureHeight, GL_RGB, GL_UNSIGNED_BYTE, f->frameData[0]));

            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT)); 
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, frameTexture));
            GL_CHECK(glBindVertexArray(vertexArray));
            GL_CHECK(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, ((char *)NULL + (0))));
            GL_CHECK(glBindVertexArray(0));

            return result;
        }

        virtual Result SetTextureSize(uint32_t width, uint32_t height)
        {
            Result result;

            textureWidth = width;
            textureHeight = height;

            // vertexArray
            GL_CHECK(glBindVertexArray(vertexArray));
            
            // vertexBuffer
            WriteVertexBuffer(0, 0, static_cast<float>(width), static_cast<float>(height));

            GL_CHECK(glVertexAttribPointer(attribs[VERTICES], 3, GL_FLOAT, GL_FALSE, 20, ((char *)NULL + (0))));
            GL_CHECK(glEnableVertexAttribArray(attribs[VERTICES]));

            GL_CHECK(glVertexAttribPointer(attribs[TEX_COORDS], 2, GL_FLOAT, GL_FALSE, 20,  ((char *)NULL + (12))));
            GL_CHECK(glEnableVertexAttribArray(attribs[TEX_COORDS]));  

            GL_CHECK(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffer));
            unsigned char elem[6] = {
                0, 1, 2,
                0, 2, 3
            };
            GL_CHECK(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elem), elem, GL_STATIC_DRAW));
            GL_CHECK(glBindVertexArray(0));

            GL_CHECK(glActiveTexture(GL_TEXTURE0));
            GL_CHECK(glBindTexture(GL_TEXTURE_2D, frameTexture));
            GL_CHECK(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
            GL_CHECK(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
            GL_CHECK(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 
                0, GL_RGB, GL_UNSIGNED_BYTE, NULL));
            GL_CHECK(glUniform1i(uniforms[FRAME_TEX], 0));
            
            WriteMVPMatrix(width, height);

            return result;
        }

        virtual Result SetWindowSize(uint32_t windowWidth, uint32_t windowHeight)
        {
            Result result;

            float x1, x2, y1, y2;
            GetImageCoord(windowWidth, windowHeight, textureWidth, textureHeight, x1, x2, y1, y2);

            GL_CHECK(glViewport(0,0, windowWidth, windowHeight));

            WriteVertexBuffer(x1, y1, x2, y2);
            WriteMVPMatrix(windowWidth, windowHeight);

            return result;
        }


   private:
        void WriteMVPMatrix(uint32_t width, uint32_t height)
        {
            glm::mat4 mvp = glm::ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -1.0f, 1.0f);
            GL_CHECK(glUniformMatrix4fv(uniforms[MVP_MATRIX], 1, GL_FALSE, glm::value_ptr(mvp)));
        }

        void WriteVertexBuffer(float x1, float y1, float x2, float y2)
        {
            GL_CHECK(glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer));
            float quad[20] = {
                // x      y     z     u      v
                  x1,   y2,   0.0f, 0.0f, 0.0f,
                  x1,   y1,   0.0f, 0.0f, 1.0f,
                  x2,   y1,   0.0f, 1.0f, 1.0f,
                  x2,   y2,  0.0f, 1.0f, 0.0f
            };
            GL_CHECK(glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW));
        }

        enum Attribs {
            VERTICES = 0,
            TEX_COORDS,
            NB_ATTRIBS
        };

        enum Uniforms {
            MVP_MATRIX = 0,
            FRAME_TEX,
            NB_UNIFORMS
        };

        // texture size
        GLuint textureWidth;
        GLuint textureHeight;

        // video size
        GLuint windowWidth;
        GLuint windowHeight;

        // rendering
        GLuint vertexArray;
        GLuint vertexBuffer;
        GLuint elementBuffer;
        GLuint frameTexture;
        GLuint program;
        GLuint attribs[NB_ATTRIBS];
        GLuint uniforms[NB_UNIFORMS];
    };

    // Yuv420pRenderer
    class Yuv420pRenderer : public videodevice::Renderer
    {
    public:
        Yuv420pRenderer()
        : textureWidth(0),
          textureHeight(0),
          windowWidth(0),
          windowHeight(0),
          x1(0.0f),
          x2(0.0f),
          y1(0.0f),
          y2(0.0f),
          vertexArray(0),
          yTexture(0),
          uTexture(0),
          vTexture(0),
          prog(0),
          pos(0)
        {
        }

        virtual ~Yuv420pRenderer()
        {
            GL_CHECK(glDeleteVertexArrays(1, &vertexArray));
            GL_CHECK(glDeleteTextures(1, &yTexture));
            GL_CHECK(glDeleteTextures(1, &uTexture));
            GL_CHECK(glDeleteTextures(1, &vTexture));
        }
        
        virtual Result Create()
        {
            Result result;

            logger::Info("Creating YUV420P Renderer");

            const std::string vertexShaderSource = "" 
            "#version 330\n"
            ""
            "uniform mat4 mvpMatrix;"
            "uniform vec4 pos;"
            ""
            "const vec2 verts[4] = vec2[] ("
            "  vec2(-0.5,  0.5), "
            "  vec2(-0.5, -0.5), "
            "  vec2( 0.5,  0.5), "
            "  vec2( 0.5, -0.5)  "
            ");"
            ""
            "const vec2 texcoords[4] = vec2[] ("
            "  vec2(0.0, 1.0), "
            "  vec2(0.0, 0.0), "
            "  vec2(1.0, 1.0), "
            "  vec2(1.0, 0.0)  "
            "); "
            ""
            "out vec2 vcoord; "
            ""
            "void main() {"
            "   vec2 vert = verts[gl_VertexID];"
            "   vec4 p = vec4((0.5 * pos.z) + pos.x + (vert.x * pos.z), "
            "                 (0.5 * pos.w) + pos.y + (vert.y * pos.w), "
            "                 0, 1);"
            "   gl_Position = mvpMatrix * p;"
            "   vcoord = texcoords[gl_VertexID];" 
            "}"
            "";

            const std::string fragmentShaderSource = ""
            "#version 330\n"
            "uniform sampler2D yTexture;"
            "uniform sampler2D uTexture;"
            "uniform sampler2D vTexture;"
            "in vec2 vcoord;"
            "layout( location = 0 ) out vec4 fragcolor;"
            ""
            "const vec3 R_cf = vec3(1.164383,  0.000000,  1.596027);"
            "const vec3 G_cf = vec3(1.164383, -0.391762, -0.812968);"
            "const vec3 B_cf = vec3(1.164383,  2.017232,  0.000000);"
            "const vec3 offset = vec3(-0.0625, -0.5, -0.5);"
            ""
            "void main() {"
            "  float y = texture(yTexture, vcoord).r;"
            "  float u = texture(uTexture, vcoord).r;"
            "  float v = texture(vTexture, vcoord).r;"
            "  vec3 yuv = vec3(y,u,v);"
            "  yuv += offset;"
            "  fragcolor = vec4(0.0, 0.0, 0.0, 1.0);"
            "  fragcolor.r = dot(yuv, R_cf);"
            "  fragcolor.g = dot(yuv, G_cf);"
            "  fragcolor.b = dot(yuv, B_cf);"
            "}"
            "";

            result = BuildProgram(vertexShaderSource, fragmentShaderSource, prog);
            if(!result)
            {
                return result;
            }

            glUseProgram(prog);

            glUniform1i(glGetUniformLocation(prog, "yTexture"), 0);
            glUniform1i(glGetUniformLocation(prog, "uTexture"), 1);
            glUniform1i(glGetUniformLocation(prog, "vTexture"), 2);

            pos = glGetUniformLocation(prog, "pos");

            glGenTextures(1, &yTexture);
            glGenTextures(1, &vTexture);
            glGenTextures(1, &uTexture);
            glGenVertexArrays(1, &vertexArray);

            return result;
        }

        virtual Result Draw(videodevice::FrameBuffer* f)
        {
            Result result;

            GL_CHECK(glClear(GL_COLOR_BUFFER_BIT)); 

            glUseProgram(prog);

            glBindTexture(GL_TEXTURE_2D, yTexture);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, f->lineSize[0]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, f->width, f->height, GL_RED, GL_UNSIGNED_BYTE, f->frameData[0]);

            glBindTexture(GL_TEXTURE_2D, uTexture);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, f->lineSize[1]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, f->width/2, f->height/2, GL_RED, GL_UNSIGNED_BYTE, f->frameData[1]);

            glBindTexture(GL_TEXTURE_2D, vTexture);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, f->lineSize[2]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, f->width/2, f->height/2, GL_RED, GL_UNSIGNED_BYTE, f->frameData[2]);

            glBindVertexArray(vertexArray);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, yTexture);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, uTexture);

            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, vTexture);

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            return result;
        }

        virtual Result SetTextureSize(uint32_t width, uint32_t height)
        {
            Result result;

            textureWidth = width;
            textureHeight = height;

            glBindTexture(GL_TEXTURE_2D, yTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, NULL); // y_pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindTexture(GL_TEXTURE_2D, uTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glBindTexture(GL_TEXTURE_2D, vTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width/2, height/2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            return result;
        }

        virtual Result SetWindowSize(uint32_t w, uint32_t h)
        {
            Result result;

            windowWidth = w;
            windowHeight = h;

            GetImageCoord(windowWidth, windowHeight, textureWidth, textureHeight, x1, x2, y1, y2);
            glUniform4f(pos, x1, y1, x2-x1, y2-y1);

            GL_CHECK(glViewport(0,0, windowWidth, windowHeight));
            WriteMVPMatrix(windowWidth, windowHeight);

            return result;
        }

   private:
        void WriteMVPMatrix(uint32_t width, uint32_t height)
        {
            glm::mat4 mvp = glm::ortho(0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, -1.0f, 1.0f);
            GL_CHECK(glUniformMatrix4fv(glGetUniformLocation(prog, "mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvp)));
        }

        // texture size
        GLuint textureWidth;
        GLuint textureHeight;

        // video size
        GLuint windowWidth;
        GLuint windowHeight;

        // draw position
        float x1;
        float x2;
        float y1;
        float y2;

        // rendering
        GLuint vertexArray;

        // yuv textures
        GLuint yTexture;
        GLuint uTexture;
        GLuint vTexture;

        // shader
        GLuint prog;

        // draw position
        GLint  pos;
    };
    // Yuv420pRenderer End
}

namespace videodevice
{

    // videodevice
    Result Init()
    {
        return Result();
    }

    void GetSupportedFormat(VideoFormatList& l)
    {
        l.push_back(VF_RGB24);
        l.push_back(VF_YUV420P);
    }

    Result Create(Device*& device, VideoFormat outputFormat)
    {
        Result result;

        if( currentDevice != NULL )
        {
            return Result(false, "Video device already exist. Cannot create more than one device.");
        }

        // must be done after context creation on windows
        InitGLext();

        device = new Device();
        memset(device, 0, sizeof(Device));

        // only one OpenGL device exists at one time
        currentDevice = device;

        // initialize renderer
        switch(outputFormat)
        {
            case VF_RGB24:
                device->renderer = new Rgb24Renderer();
                break;

            case VF_YUV420P:
               device->renderer = new Yuv420pRenderer();
               break;

            default:
                return Result(false, "Unsupported output format %d", outputFormat);
        }

        result = device->renderer->Create();
        if(!result)
        {
            return result;
        }

        return result;
    }

    Result DrawFrame(Device* device, FrameBuffer* fb)
    {
        return device->renderer->Draw(fb);
    }

    Result SetTextureSize(Device* device, uint32_t width, uint32_t height)
    {
        Result result;

        device->width = width;
        device->height = height;

        result = device->renderer->SetTextureSize(width,height);

        return result;
    }

    Result SetWindowSize(Device* device, uint32_t width, uint32_t height)
    {
        Result result;

        device->windowWidth = width;
        device->windowHeight = height;

        result = device->renderer->SetWindowSize(width,height);

        return result;
    }

    void Destroy(Device*& device)
    {
        if(device)
        {
            delete device->renderer;
            delete device;
            device = NULL;
            currentDevice = NULL;
        }
    }

}




