#include "resource_manager.h"

#include <stdio.h>

#define USE_STB 1

#if USE_STB
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_PSD
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
//#include <stb_image_aug.h>
#else
#include <SOIL.h>
#endif

#define MAX_RESOURCE 8

struct shader_resource {
	const char* shader_name;
	shader_t* shader_file;
};

struct texture_resource {
	const char* texture_name;
	texture_t* texture_file;
};

struct resource_manager {
	texture_resource* textures;
	shader_resource* shaders;
	int shader_count;
	int texture_count;
};

/* Local Function Prototypes */
static shader_t* loadShaderFromFile(const GLchar* vShaderFile, const GLchar *fShaderFile, const GLchar *gShaderFile);
static texture_t* loadTextureFromFile(const GLchar* file, GLboolean alpha);
static char* readFromFile(const GLchar* filepath);

static resource_manager rm;

void resource_initManager() {
	rm.textures = (texture_resource*)malloc(sizeof(texture_resource) * MAX_RESOURCE);
	rm.shaders = (shader_resource*)malloc(sizeof(shader_resource) * MAX_RESOURCE);
	rm.shader_count = 0;
	rm.texture_count = 0;

	memset(rm.textures, 0, sizeof(rm.textures));
	memset(rm.shaders, 0, sizeof(rm.shaders));
};

void resource_destroy() {
	for (int i = 0; i < rm.shader_count; ++i) {
		if (rm.shaders[i].shader_file != NULL) {
			free(rm.shaders[i].shader_file);
		}
	}

	for (int j = 0; j < rm.texture_count; ++j) {
		if (rm.textures[j].texture_file != NULL) {
			free(rm.textures[j].texture_file);
		}
	}

	free(rm.shaders);
	free(rm.textures);
}

shader_t* resource_loadShader(const GLchar *vShaderFile, const GLchar *fShaderFile, const GLchar *gShaderFile, const char* name)
{
	if (rm.shader_count == MAX_RESOURCE) {
		fprintf(stderr, "Maximum shaders reached.\n");
		fflush(stderr);
		return NULL;
	}

	shader_t* s = loadShaderFromFile(vShaderFile, fShaderFile, gShaderFile);
	rm.shaders[rm.shader_count].shader_file = s;
	rm.shaders[rm.shader_count].shader_name = name;
	rm.shader_count++;
	return s;
}

void resource_destroyShader(char* name)
{
	shader_t* s = resource_getShader(name);
	if (s != NULL) {
		shader_destroy(s);
	}
}

shader_t* resource_getShader(char* name)
{
	int ret;
	for (int i = 0; i < rm.shader_count; ++i) {
		ret = strcmp(rm.shaders[i].shader_name, name);
		if (ret == 0)
		{
			return rm.shaders[i].shader_file;
		}
	}

	fprintf(stderr, "Shader '%s' not found.\n", name);
	return NULL;
}

texture_t* resource_loadTexture(const GLchar *file, GLboolean alpha, const char* name)
{
	if (rm.texture_count == MAX_RESOURCE) {
		fprintf(stderr, "Maximum textures reached.\n");
		fflush(stderr);
		return NULL;
	}

	texture_t* t = loadTextureFromFile(file, alpha);
	rm.textures[rm.texture_count].texture_file = t;
	rm.textures[rm.texture_count].texture_name = name;
	rm.texture_count++;
	return t;
}

void resource_destroyTexture(char* name)
{
	texture_t* t = resource_getTexture(name);
	if (t != NULL) {
		texture_destroy(t);
	}
}

texture_t* resource_getTexture(char* name)
{
	int ret;
	for (int i = 0; i < rm.texture_count; i++)
	{
		ret = strcmp(rm.textures[i].texture_name, name);
		if (ret == 0) {
			return rm.textures[i].texture_file;
		}
	}

	fprintf(stderr, "Texture '%s' not found.\n", name);
	return NULL;
}

void resource_clear()
{
	// delete all shaders/textures
	for (int i = 0; i < rm.shader_count; ++i) {
		glDeleteProgram(rm.shaders[i].shader_file->ID);
	}

	for (int i = 0; i < rm.texture_count; ++i) {
		glDeleteTextures(1, &(rm.textures[i].texture_file->ID));
	}
}

// TODO: Seperate thread to avoid I/O blocking?
static shader_t* loadShaderFromFile(const GLchar* vShaderFile, const GLchar *fShaderFile, const GLchar *gShaderFile)
{
	// read vert/frag source from file
	char *vertCode, *fragCode, *geomCode;

	vertCode = readFromFile(vShaderFile);
	fragCode = readFromFile(fShaderFile);
	if (gShaderFile != NULL) {
		geomCode = readFromFile(gShaderFile);
	}
	else {
		geomCode = NULL;
	}		
	
	if (vertCode == NULL || fragCode == NULL) {
		fprintf(stderr, "ERROR::SHADER: Failed to read shader files\n");
		fflush(stderr);
	}

	const GLchar* vShaderCode = vertCode;
	const GLchar* fShaderCode = fragCode;
	const GLchar* gShaderCode = geomCode;

	// create shader object from source
	shader_t* shader = shader_new();
	shader_compile(shader, vShaderCode, fShaderCode, gShaderCode);

	free(vertCode);
	free(fragCode);
	if (geomCode != NULL) {
		free(geomCode);
	}
	return shader;
}

static texture_t* loadTextureFromFile(const GLchar* file, GLboolean alpha)
{
	// create texture object
	texture_t* texture = texture_new();
	if (alpha) {
		texture->internal_fmt = GL_RGBA;
		texture->image_fmt = GL_RGBA;
	}

	// load image
	int width, height, comp;
	unsigned char* data = NULL;

	//data = SOIL_load_image(file, &width, &height, 0, texture->image_fmt == GL_RGBA ? SOIL_LOAD_RGBA : SOIL_LOAD_RGB);
	data = stbi_load((char*)file, &width, &height, &comp, alpha ? STBI_rgb_alpha : STBI_rgb);
	//fprintf(stderr, stbi_failure_reason());
	if (!data) {
		fprintf(stderr, "Failed to load texture.\n");
		return NULL;
	}

	// generate texture
	texture_generate(texture, width, height, data);

	// free image data
	stbi_image_free(data);
	return texture;
}

static char* readFromFile(const GLchar* filepath)
{
	long fileSize;
	char* buffer = NULL;
	size_t result;

	if (filepath == NULL) return NULL;

	// open file
#ifdef _WIN32
	FILE* fp;
	fopen_s(&fp, filepath, "rb");
#endif
#ifdef linux
	FILE* fp = fopen(filepath, "rb");
#endif
	if (fp != NULL) {
		// get the file size
		if (fseek(fp, 0L, SEEK_END) == 0) {
			fileSize = ftell(fp);
			rewind(fp);

			if (fileSize == -1) {
				fprintf(stderr, "File Size error\n");
				perror("ftell");
				fclose(fp);
				return NULL;
			}


			// allocate memory for the file contents
			buffer = (char*)malloc(sizeof(char) * (fileSize + 1));
			memset(buffer, '\0', sizeof(buffer));

			if (buffer == NULL) {
				fprintf(stderr, "Memory error\n");
				fflush(stderr);
			}

			// read files contents
			result = fread(buffer, sizeof(char), fileSize, fp);
			if (result != fileSize) {
				fprintf(stderr, "Reading error\n");
				perror("fread");
				fflush(stderr);
			}

			buffer[result++] = '\0';

			fprintf(stdout, "DEBUG: '%s' contents:\n %s\n\n", filepath, buffer);
		}

		fclose(fp);
	}
	else {
		fprintf(stderr, "File error\n");
		fflush(stderr);
	}

	return buffer;
}