
// --------------------------------------------------------------------------
// GPU raycasting tutorial
// Made by Peter Trier jan 2007
//
// This file contains all the elements nessesary to implement a simple 
// GPU volume raycaster.
// Notice this implementation requires a shader model 3.0 gfxcard
// --------------------------------------------------------------------------

#include <GL/glew.h>


#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glut.h>
#endif


#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <ctime>
#include <cassert>
#include <string.h>

#include "Vector3.h"

#define MAX_KEYS 256
#define WINDOW_SIZE 800
#define VOLUME_TEX_SIZE 128

using namespace std;

//--------------------------------------------------------------------------------------
// vertex shader
//--------------------------------------------------------------------------------------
static const char* vert = "                          \n \
varying vec4 model_view;                             \n \
                                                     \n \
void main( void )                                    \n \
{                                                    \n \
    gl_Position = ftransform();                      \n \
    model_view = gl_Position;                        \n \
    gl_TexCoord[0] = gl_MultiTexCoord1;              \n \
}"; 

//--------------------------------------------------------------------------------------
// fragment shader
//--------------------------------------------------------------------------------------
static const char* frag = "                                                 \n\
                                                                            \n\
uniform sampler2D   tex;                                                    \n\
uniform sampler3D   volume_tex;                                             \n\
uniform float   stepsize;                                                   \n\
                                                                            \n\
varying vec4 model_view;                                                    \n\
                                                                            \n\
void main( void )                                                           \n\
{                                                                           \n\
    vec2 texc = ( model_view.xy / model_view.w + 1.0 ) / 2.0 ;              \n\
    vec4 start = gl_TexCoord[0];                                            \n\
    vec4 back_position = texture2D( tex, texc );                            \n\
    vec3 dir = vec3( 0.0 );                                                 \n\
    dir.x = back_position.x - start.x;                                      \n\
    dir.y = back_position.y - start.y;                                      \n\
    dir.z = back_position.z - start.z;                                      \n\
    float len = length( dir.xyz );                                          \n\
    vec3 norm_dir = normalize( dir );                                       \n\
    float delta = stepsize;                                                 \n\
    vec3 delta_dir = norm_dir * delta;                                      \n\
    float delta_dir_len = length( delta_dir );                              \n\
    vec3 vect = start.xyz;                                                  \n\
    vec4 col_acc = vec4( 0., 0., 0., 0. );                                  \n\
    float alpha_acc = 0.0;                                                  \n\
    float length_acc = 0.0;                                                 \n\
    vec4 color_sample;                                                      \n\
    float alpha_sample;                                                     \n\
                                                                            \n\
    for( int i = 0; i < 450; i++ )                                          \n\
    {                                                                       \n\
        color_sample = texture3D( volume_tex, vect );                       \n\
        alpha_sample = color_sample.a * stepsize;                           \n\
        col_acc += ( 1. - alpha_acc ) * color_sample * alpha_sample * 3.;    \n\
        alpha_acc += alpha_sample;                                          \n\
        vect += delta_dir;                                                  \n\
        length_acc += delta_dir_len;                                        \n\
        if( length_acc > len || alpha_acc > 1.0 )                          \n\
            break;                                                          \n\
    }                                                                       \n\
    gl_FragColor =  col_acc;                                                \n\
                                                                            \n\
}";

//--------------------------------------------------------------------------------------
// global variables
//--------------------------------------------------------------------------------------
GLuint g_shaderProgram = 0;
    
bool gKeys[MAX_KEYS];
bool toggle_visuals = true;
GLuint renderbuffer; 
GLuint framebuffer; 
GLuint volume_texture; // the volume texture
GLuint backface_buffer; // the FBO buffers
GLuint final_image;
float stepsize = 1.0/50.0;

//--------------------------------------------------------------------------------------
// add shader
//--------------------------------------------------------------------------------------
void add_shader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
    GLuint ShaderObj = glCreateShader(ShaderType);
    if (ShaderObj == 0)
    {
        cout<<"Error creating shader type : "<<ShaderType<<endl;
        return;
    }
    
    const GLchar* p[1];
    p[0] = pShaderText;
    GLint Lengths[1];
    Lengths[0]= strlen(pShaderText);
    glShaderSource(ShaderObj, 1, p, Lengths);
    glCompileShader(ShaderObj);
    GLint success;
    glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        GLchar InfoLog[1024];
        glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
        cout<<"Error compiling shader type : "<<ShaderType<<" : "<<InfoLog<<endl;
        return;
    }
    
    glAttachShader(ShaderProgram, ShaderObj);
}

//--------------------------------------------------------------------------------------
// compile shader
//--------------------------------------------------------------------------------------
static void compile_shaders()
{
    g_shaderProgram = glCreateProgram();
    
    if (g_shaderProgram == 0)
    {
        cout<<"Error creating shader program! "<<endl;
        return;
    }
    
    add_shader(g_shaderProgram, vert, GL_VERTEX_SHADER);
    add_shader(g_shaderProgram, frag, GL_FRAGMENT_SHADER);
    
    GLint Success = 0;
    GLchar ErrorLog[1024] = { 0 };
    
    glLinkProgram(g_shaderProgram);
    glGetProgramiv(g_shaderProgram, GL_LINK_STATUS, &Success);
    if (Success == 0)
    {
        glGetProgramInfoLog(g_shaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
        cout<< "Error linking shader program : "<<ErrorLog<<endl;
        return;
    }
    
    return;
    
  
}

//--------------------------------------------------------------------------------------
// validate shader
//--------------------------------------------------------------------------------------
void validate_shader( GLint i_program )
{
    GLint Success = 0;
    GLchar ErrorLog[1024] = { 0 };
    
    glValidateProgram(g_shaderProgram);
    glGetProgramiv(g_shaderProgram, GL_VALIDATE_STATUS, &Success);
    if (!Success)
    {
        glGetProgramInfoLog(g_shaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
        cout<<" Invalid shader program: "<< ErrorLog<<endl;
        return;
    }  
}

//--------------------------------------------------------------------------------------
//  enable render buffers
//--------------------------------------------------------------------------------------
void enable_renderbuffers()
{
	glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, framebuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
}

//--------------------------------------------------------------------------------------
// disable render buffers
//--------------------------------------------------------------------------------------
void disable_renderbuffers()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

//--------------------------------------------------------------------------------------
//  vertex function
//--------------------------------------------------------------------------------------
void vertex(float x, float y, float z)
{
	glColor3f(x,y,z);
	glMultiTexCoord3fARB(GL_TEXTURE1_ARB, x, y, z);
	glVertex3f(x,y,z);
}

//--------------------------------------------------------------------------------------
// this method is used to draw the front and backside of the volume
//--------------------------------------------------------------------------------------
void drawQuads(float x, float y, float z)
{
	
	glBegin(GL_QUADS);
	/* Back side */
	glNormal3f(0.0, 0.0, -1.0);
	vertex(0.0, 0.0, 0.0);
	vertex(0.0, y, 0.0);
	vertex(x, y, 0.0);
	vertex(x, 0.0, 0.0);

	/* Front side */
	glNormal3f(0.0, 0.0, 1.0);
	vertex(0.0, 0.0, z);
	vertex(x, 0.0, z);
	vertex(x, y, z);
	vertex(0.0, y, z);

	/* Top side */
	glNormal3f(0.0, 1.0, 0.0);
	vertex(0.0, y, 0.0);
	vertex(0.0, y, z);
    vertex(x, y, z);
	vertex(x, y, 0.0);

	/* Bottom side */
	glNormal3f(0.0, -1.0, 0.0);
	vertex(0.0, 0.0, 0.0);
	vertex(x, 0.0, 0.0);
	vertex(x, 0.0, z);
	vertex(0.0, 0.0, z);

	/* Left side */
	glNormal3f(-1.0, 0.0, 0.0);
	vertex(0.0, 0.0, 0.0);
	vertex(0.0, 0.0, z);
	vertex(0.0, y, z);
	vertex(0.0, y, 0.0);

	/* Right side */
	glNormal3f(1.0, 0.0, 0.0);
	vertex(x, 0.0, 0.0);
	vertex(x, y, 0.0);
	vertex(x, y, z);
	vertex(x, 0.0, z);
	glEnd();
	
}
//--------------------------------------------------------------------------------------
// create a test volume texture, here you could load your own volume
//--------------------------------------------------------------------------------------
void create_volumetexture()
{
	int size = VOLUME_TEX_SIZE*VOLUME_TEX_SIZE*VOLUME_TEX_SIZE* 4;
	GLubyte *data = new GLubyte[size];
    
    const int UPPER = VOLUME_TEX_SIZE *2 - 6;

	for(int x = 0; x < VOLUME_TEX_SIZE; x++)
	{
        for(int y = 0; y < VOLUME_TEX_SIZE; y++)
        {
            for(int z = 0; z < VOLUME_TEX_SIZE; z++)
            {
                int r = (x*4)   + (y * VOLUME_TEX_SIZE * 4) + (z * VOLUME_TEX_SIZE * VOLUME_TEX_SIZE * 4);
                int g = r+1;
                int b = g+1;
                int a = b+1;
                
                data[ r ] = z; //z%UPPER;
                data[ g ] = y; // y%UPPER;
                data[ b ] = UPPER;
                data[ a ] = UPPER-20;
                
                Vector3 p =	Vector3(x,y,z)- Vector3(VOLUME_TEX_SIZE-20,VOLUME_TEX_SIZE-30,VOLUME_TEX_SIZE-30);
                
                bool test = (p.length() < 42);
                if(test)
                {
                    data[ a ] = 0;
                }

                p =	Vector3(x,y,z)- Vector3(VOLUME_TEX_SIZE/2,VOLUME_TEX_SIZE/2,VOLUME_TEX_SIZE/2);
                test = (p.length() < 24);
                if(test)
                    data[ a ] = 0;

                
                if(x > 20 && x < 40 && y > 0 && y < VOLUME_TEX_SIZE && z > 10 &&  z < 50)
                {
                    data[ r ] = UPPER/2;
                    data[ g ] = UPPER;
                    data[ b ] = y%(UPPER/2);
                    data[ a ] = UPPER;
                }

                if(x > 50 && x < 70 && y > 0 && y < VOLUME_TEX_SIZE && z > 10 &&  z < 50)
                {
                    data[ r ] = UPPER;
                    data[ g ] = UPPER;
                    data[ b ] = y%(UPPER/2);
                    data[ a ] = UPPER; 
                }

                if(x > 80 && x < 100 && y > 0 && y < VOLUME_TEX_SIZE && z > 10 &&  z < 50)
                {
                    data[ r ] = UPPER;
                    data[ g ] = UPPER/3;
                    data[ b ] = y%(UPPER/2);
                    data[ a ] = UPPER;
                }

                p =	Vector3(x,y,z)- Vector3(24,24,24);
                test = (p.length() < 40);
                if(test)
                    data[ a ] = 0;
            }
        }
    }

	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1, &volume_texture);
	glBindTexture(GL_TEXTURE_3D, volume_texture);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
	
    glTexImage3D(GL_TEXTURE_3D, 0,GL_RGBA, VOLUME_TEX_SIZE, VOLUME_TEX_SIZE,VOLUME_TEX_SIZE,0, GL_RGBA, GL_UNSIGNED_BYTE,data);
    
	delete []data;
	cout << "volume texture created" << endl;

}

//--------------------------------------------------------------------------------------
// ok let's start things up 
//--------------------------------------------------------------------------------------
void init()
{
	cout << "glew init " << endl;
	GLenum err = glewInit();

	// initialize all the OpenGL extensions
	glewGetExtension("glMultiTexCoord2fvARB");  
	if(glewGetExtension("GL_EXT_framebuffer_object") )cout << "GL_EXT_framebuffer_object support " << endl;
	if(glewGetExtension("GL_EXT_renderbuffer_object"))cout << "GL_EXT_renderbuffer_object support " << endl;
	if(glewGetExtension("GL_ARB_vertex_buffer_object")) cout << "GL_ARB_vertex_buffer_object support" << endl;
	if(GL_ARB_multitexture)cout << "GL_ARB_multitexture support " << endl;
	
	if (glewGetExtension("GL_ARB_fragment_shader")      != GL_TRUE ||
		glewGetExtension("GL_ARB_vertex_shader")        != GL_TRUE ||
		glewGetExtension("GL_ARB_shader_objects")       != GL_TRUE ||
		glewGetExtension("GL_ARB_shading_language_100") != GL_TRUE)
	{
		cout << "Driver does not support OpenGL Shading Language" << endl;
		exit(1);
	}

	glEnable(GL_CULL_FACE);
	glClearColor(0.0, 0.0, 0.0, 0);
	create_volumetexture();

	// CG init
        
    compile_shaders();
        
	// Create the to FBO's one for the backside of the volumecube and one for the finalimage rendering
	glGenFramebuffersEXT(1, &framebuffer);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,framebuffer);

	glGenTextures(1, &backface_buffer);
	glBindTexture(GL_TEXTURE_2D, backface_buffer);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA16F_ARB, WINDOW_SIZE, WINDOW_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);
	(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, backface_buffer, 0);

	glGenTextures(1, &final_image);
	glBindTexture(GL_TEXTURE_2D, final_image);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA16F_ARB, WINDOW_SIZE, WINDOW_SIZE, 0, GL_RGBA, GL_FLOAT, NULL);

	glGenRenderbuffersEXT(1, &renderbuffer);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, renderbuffer);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH_COMPONENT, WINDOW_SIZE, WINDOW_SIZE);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, renderbuffer);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	
}

//--------------------------------------------------------------------------------------
// for contiunes keypresses
//--------------------------------------------------------------------------------------
void ProcessKeys()
{
	// Process keys
	for (int i = 0; i < 256; i++)
	{
		if (!gKeys[i])  { continue; }
		switch (i)
		{
		case ' ':
			break;
		case 'w':
			stepsize += 1.0/2048.0;
			if(stepsize > 0.25) stepsize = 0.25;
			break;
		case 'e':
			stepsize -= 1.0/2048.0;
			if(stepsize <= 1.0/200.0) stepsize = 1.0/200.0;
			break;
		}
	}

}

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------
void key(unsigned char k, int x, int y)
{
	gKeys[k] = true;
}

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------
void KeyboardUpCallback(unsigned char key, int x, int y)
{
	gKeys[key] = false;

	switch (key)
	{
	case 27 :
		{
			exit(0); break; 
		}
	case ' ':
		toggle_visuals = !toggle_visuals;
		break;
	}
}

//--------------------------------------------------------------------------------------
// glut idle function
//--------------------------------------------------------------------------------------
void idle_func()
{
	ProcessKeys();
	glutPostRedisplay();
}

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------
void reshape_ortho(int w, int h)
{
	if (h == 0) h = 1;
	glViewport(0, 0,w,h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(0, 1, 0, 1);
	glMatrixMode(GL_MODELVIEW);
}
//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------
void resize(int w, int h)
{
	if (h == 0) h = 1;
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, (GLfloat)w/(GLfloat)h, 0.01, 400.0);
	glMatrixMode(GL_MODELVIEW);
}

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------
void draw_fullscreen_quad()
{
	glDisable(GL_DEPTH_TEST);
	glBegin(GL_QUADS);
   
	glTexCoord2f(0,0); 
	glVertex2f(0,0);

	glTexCoord2f(1,0); 
	glVertex2f(1,0);

	glTexCoord2f(1, 1); 

	glVertex2f(1, 1);
	glTexCoord2f(0, 1); 
	glVertex2f(0, 1);

	glEnd();
	glEnable(GL_DEPTH_TEST);

}

//--------------------------------------------------------------------------------------
// display the final image on the screen
//--------------------------------------------------------------------------------------
void render_buffer_to_screen()
{
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glLoadIdentity();
	glEnable(GL_TEXTURE_2D);
	if(toggle_visuals)
		glBindTexture(GL_TEXTURE_2D,final_image);
	else
		glBindTexture(GL_TEXTURE_2D,backface_buffer);
	reshape_ortho(WINDOW_SIZE,WINDOW_SIZE);
	draw_fullscreen_quad();
	glDisable(GL_TEXTURE_2D);
}

//--------------------------------------------------------------------------------------
// render the backface to the offscreen buffer backface_buffer
//--------------------------------------------------------------------------------------
void render_backface()
{
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, backface_buffer, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);
	drawQuads(1.0,1.0, 1.0);
	glDisable(GL_CULL_FACE);
}

//--------------------------------------------------------------------------------------
//
//--------------------------------------------------------------------------------------
void raycasting_pass()
{
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, final_image, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    
    // set step size: 
    glUseProgram( g_shaderProgram );
    //glBindParameterEXT( g_shaderProgram );
    glUniform1f( glGetUniformLocation( g_shaderProgram, "stepsize" ), stepsize );

    // set backface texture 
    glActiveTexture(GL_TEXTURE0 );
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, backface_buffer);
    glUniform1i(glGetUniformLocation( g_shaderProgram, "tex" ), 0 ); 
    
    if( glGetError() != GL_NO_ERROR ) cout<<" pass 2D texture is wrong..."<<endl;

    
    // set 3D volume textures:
    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_3D);
    glBindTexture(GL_TEXTURE_3D, volume_texture);
    glUniform1i( glGetUniformLocation( g_shaderProgram, "volume_tex" ) , 1 ); 
    
    if( glGetError() != GL_NO_ERROR ) cout<<" pass 3D texture is wrong..."<<endl;
    
    // validate shader program
    validate_shader( g_shaderProgram );
    
    //
    glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);
	drawQuads(1.0,1.0, 1.0);
	glDisable(GL_CULL_FACE);
	
    glUseProgram(0);
    glActiveTexture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_3D);
    glActiveTexture(GL_TEXTURE0);
    
}

//--------------------------------------------------------------------------------------
// This display function is called once pr frame 
//--------------------------------------------------------------------------------------
void display()
{
	static float rotate = 0; 
	rotate += 0.25;

	resize(WINDOW_SIZE,WINDOW_SIZE);
	enable_renderbuffers();

	glLoadIdentity();
	glTranslatef(0,0,-2.25);
	glRotatef(rotate,0,1,1);
	glTranslatef(-0.5,-0.5,-0.5); // center the texturecube
	render_backface();
	raycasting_pass();
	disable_renderbuffers();
	render_buffer_to_screen();
	glutSwapBuffers();
}

//--------------------------------------------------------------------------------------
// main
//--------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	glutInit(&argc,argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
	glutCreateWindow("GPU raycasting tutorial");
	glutReshapeWindow(WINDOW_SIZE,WINDOW_SIZE);
	glutKeyboardFunc(key);
	glutKeyboardUpFunc(KeyboardUpCallback);
	
	glutDisplayFunc(display);
	glutIdleFunc(idle_func);
	glutReshapeFunc(resize);
	resize(WINDOW_SIZE,WINDOW_SIZE);
	init();
	glutMainLoop();
	return 0;
}

