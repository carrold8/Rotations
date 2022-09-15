#define no_init_all deprecated

// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
#include "maths_funcs.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>#
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/norm.hpp>

/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
#define PLANE_MODEL "Plane_no_prop.dae"
#define PROP "prop.dae"
#define GROUND "ground.dae"
/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/



vec3 light_pos = vec3(1.2f, 1.0f, 2.0f);
vec3 blue = vec3(0.0f, 0.0f, 1.0f);
vec3 red = vec3(1.0f, 0.0f, 0.0f);
vec3 white = vec3(1.0f, 1.0f, 1.0f);


vec3 cameraPos = vec3(0.0f, 1.0f, -8.0f);
vec3 cameraTarget = vec3(0.0f, 0.0f, 0.0f);
vec3 cameraDirec = normalise(cameraPos - cameraTarget);
vec3 up = vec3(0.0f, 1.0f, 0.0f);
vec3 camRight = normalise(cross(up, cameraDirec));
vec3 cameraUp = cross(cameraDirec, camRight);
vec3 cameraFront = vec3(0.0f, 0.0f, -1.0f);


#pragma region SimpleTypes
typedef struct ModelData
{
	size_t mPointCount = 0;
	std::vector<vec3> mVertices;
	std::vector<vec3> mNormals;
	std::vector<vec2> mTextureCoords;
} ModelData;
#pragma endregion SimpleTypes

using namespace std;
GLuint shaderProgramID;
GLuint shaderProgramRED;
GLuint shaderProgramBLUE;

ModelData plane, prop, ground;
GLuint plane_vao, prop_vao, ground_vao;

bool mode = false;
bool FirstPerson;

versor QuatRotation = quat_from_axis_deg(0, 0, 0, 1);;


//ModelData mesh_data;
unsigned int mesh_vao = 0;
int width = 800;
int height = 600;

GLuint loc1, loc2, loc3;
GLfloat rotate_x = 0.0f;
GLfloat update_x = 0.0f;
GLfloat rotate_y = 0.0f;
GLfloat update_y = 0.0f;
GLfloat rotate_z = 0.0f;
GLfloat update_z = 0.0f;
GLfloat rotate_prop = 0.0f;


float x, y, z, w;
versor Orientation = quat_from_axis_deg(0.0f, 0.0f, 1.0f, 0.0f);


#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {

	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	const aiScene* scene = aiImportFile(
		file_name,
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}
	}

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING

// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders(const char* vertex, const char* fragment)
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	GLuint shaderProgram = glCreateProgram();
	if (shaderProgram == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}


	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgram, vertex, GL_VERTEX_SHADER);
	AddShader(shaderProgram, fragment, GL_FRAGMENT_SHADER);


	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgram);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgram);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgram, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgram, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgram);
	return shaderProgram;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS
GLuint generateObjectBufferMesh(ModelData mesh_data, GLuint shader) {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	//Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	//Might be an idea to do a check for that before generating and binding the buffer.



	unsigned int vp_vbo = 0;
	loc1 = glGetAttribLocation(shader, "vertex_position");
	loc2 = glGetAttribLocation(shader, "vertex_normal");
	loc3 = glGetAttribLocation(shader, "vertex_texture");

	glGenBuffers(1, &vp_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mVertices[0], GL_STATIC_DRAW);
	unsigned int vn_vbo = 0;
	glGenBuffers(1, &vn_vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glBufferData(GL_ARRAY_BUFFER, mesh_data.mPointCount * sizeof(vec3), &mesh_data.mNormals[0], GL_STATIC_DRAW);



	//	This is for texture coordinates which you don't currently need, so I have commented it out
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(loc1);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
	glVertexAttribPointer(loc1, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
	glVertexAttribPointer(loc2, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	//	This is for texture coordinates which you don't currently need, so I have commented it out

	return vao;

}
#pragma endregion VBO_FUNCTIONS


void display() {

	
	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramBLUE);



	//Declare your uniform variables that will be used in your shader
	// Vertex Shader Uniforms
	int matrix_location = glGetUniformLocation(shaderProgramBLUE, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramBLUE, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramBLUE, "proj");


	// Root of the Hierarchy
	mat4 view = identity_mat4();
	mat4 persp_proj = perspective(90.0f, (float)width / (float)height, 0.1f, 1000.0f);
	mat4 model = identity_mat4();

	mat4 rotationMatrix = quat_to_mat4(QuatRotation);
	
	//Quaternions
	if (mode == true) {
		model = model * rotationMatrix;
	}
	//Euler
	if (mode == false) {
		model = rotate_x_deg(model, rotate_x);
		model = rotate_y_deg(model, rotate_y);
		model = rotate_z_deg(model, rotate_z);
	}

	if (FirstPerson == false) {
		view = translate(view, vec3(0.0, -2.0, -10.0f));
	}
	if (FirstPerson == true) {
		vec3 CamMove = vec3(-1, -0.5, 0);
		view = rotate_y_deg(view, 180);
		view = model * view;
		view = translate(view, CamMove);
	}
	
//	view = model * view;
//	view = rotate_y_deg(view, 180);
//	view = translate(view, CamMove);


	// update uniforms & draw
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, model.m);


	glBindVertexArray(plane_vao);
	glDrawArrays(GL_TRIANGLES, 0, plane.mPointCount);



	// Trying to use second shader
	int matrix_location1 = glGetUniformLocation(shaderProgramRED, "model");
	int view_mat_location1 = glGetUniformLocation(shaderProgramRED, "view");
	int proj_mat_location1 = glGetUniformLocation(shaderProgramRED, "proj");





// Set up the child matrix
	mat4 propeller = identity_mat4();
	propeller = rotate_z_deg(propeller, rotate_prop);
	propeller = translate(propeller, vec3(0.0f, 0.0f, 2.41859f));
	propeller = model * propeller;

	// Update the appropriate uniform and draw the mesh again
	glUniformMatrix4fv(proj_mat_location1, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location1, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, propeller.m);
	glBindVertexArray(prop_vao);
	glDrawArrays(GL_TRIANGLES, 0, prop.mPointCount);


	mat4 thirdModel = identity_mat4();
	thirdModel = translate(thirdModel, vec3(0.0f, 0.0f, 0.0f));
//	thirdModel = rotate_y_deg(thirdModel, 180.0f);

	glUseProgram(shaderProgramRED);
	glUniformMatrix4fv(proj_mat_location1, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location1, 1, GL_FALSE, view.m);

	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, thirdModel.m);
	glBindVertexArray(ground_vao);
	glDrawArrays(GL_TRIANGLES, 0, ground.mPointCount);




	glutSwapBuffers();
}

double radians(double degree) {
	double pi = 3.14159265359;
	return (degree * (pi / 180));
}




void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	float delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	// Rotate the model slowly around the y axis at 20 degrees per second
	rotate_x += 20.0f * delta;
	rotate_x = fmodf(update_x, 360.0f);

	rotate_y += 20.0f * delta;
	rotate_y = fmodf(update_y, 360.0f);

	rotate_z += 20.0f * delta;
	rotate_z = fmodf(update_z, 360.0f);

	rotate_prop += 100.0f * delta;
	rotate_prop = fmodf(rotate_prop, 360.0f);

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	shaderProgramBLUE = CompileShaders("Plane_Vertex_BLUE.txt", "Plane_Fragment_BLUE.txt");
	shaderProgramRED = CompileShaders("Plane_Vertex_RED.txt", "Plane_Fragment_RED.txt");
	plane = load_mesh(PLANE_MODEL);
	plane_vao = generateObjectBufferMesh(plane, shaderProgramBLUE);
	// load mesh into a vertex buffer array
	prop = load_mesh(PROP);
	prop_vao = generateObjectBufferMesh(prop, shaderProgramRED);
	ground = load_mesh(GROUND);
	ground_vao = generateObjectBufferMesh(ground, shaderProgramRED);

}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	
	// Key press to change mode

	if (key == 'c') {
		mode = !mode;
	}

	// Change camera mode
	if (key == 'p') {
		FirstPerson = !FirstPerson;
	}

	
	// Key press for rotating plane


		// Yaw Controls
	if (key == 'q') {
		update_y += 20.0f;
		versor quat_y = quat_from_axis_deg(20, 0, 1, 0);
		QuatRotation = QuatRotation * quat_y;
	}
	if (key == 'e') {
		update_y -= 20.0f;
		versor quat_y = quat_from_axis_deg(-20, 0, 1, 0);
		QuatRotation = QuatRotation * quat_y;
	}

	// Pitch Controls
	if (key == 'w') {
		update_x += 20.0f;
		versor quat_x = quat_from_axis_deg(20, 1, 0, 0);
		QuatRotation = QuatRotation * quat_x;
	}
	if (key == 's') {
		update_x -= 20.0f;
		versor quat_x = quat_from_axis_deg(-20, 1, 0, 0);
		QuatRotation = QuatRotation * quat_x;
	}

	// Roll Controls
	if (key == 'a') {
		update_z += 20.0f;
		versor quat_z = quat_from_axis_deg(20, 0, 0, 1);
		QuatRotation = QuatRotation * quat_z;
	}
	if (key == 'd') {
		update_z -= 20.0f;
		versor quat_z = quat_from_axis_deg(-20, 0, 0, 1);
		QuatRotation = QuatRotation * quat_z;
	}
}

int main(int argc, char** argv) {

	int setup;

	cout << "Choose to rotate using Euler[0] or Quaternions[1]: \n";
	cin >> setup;

	while (setup != 0 && setup != 1) {
		cout << "Choose to rotate using Euler[0] or Quaternions[1]: \n";
		cin >> setup;
	}

	cout << "****	Change Rotation Method by pressing c	***** \n\n";

	cout << "Pitch			Roll		Yaw\nW + S			A + D		Q + E\n\n";

	if (setup == 1) {
		mode = true;
	}

	
	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Hello Triangle");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}