#include "shader.h"

Shader::Shader(string name, function<void(GLuint, Object*, TriangleMesh*)> setup, string vertexFilename, string fragmentFilename, string geometryFilename) {

	string vsString, fsString, gsString, line;
	const GLchar *vsSource, *fsSource, *gsSource;
	GLint isCompiled, vs, fs, gs;
	ifstream file;

	program = glCreateProgram();
	this->name = name;
	this->setup = setup;
	glGenBuffers(1, &vbo);

	// Vertex shader
	file.open(vertexFilename);
	vsString = "";
	while (getline(file, line))
		vsString += line + '\n';
	file.close();

	vs = glCreateShader(GL_VERTEX_SHADER);
	vsSource = vsString.c_str();
	glShaderSource(vs, 1, &vsSource, NULL);
	glCompileShader(vs);
	glGetShaderiv(vs, GL_COMPILE_STATUS, &isCompiled);
	glAttachShader(program, vs);

	if (!isCompiled) {
		GLint errorLength;
		string error;
		glGetShaderiv(vs, GL_INFO_LOG_LENGTH, &errorLength);
		error.resize(errorLength);
		glGetShaderInfoLog(vs, errorLength, 0, &error[0]);
		cout << "VERTEX SHADER COMPILATION ERROR!" << endl << error << " in " << vertexFilename << endl << endl;
	}

	// Geometry shader (optional)
	if (geometryFilename != "") {
		file.open(geometryFilename);
		gsString = "";
		while (getline(file, line))
			gsString += line + '\n';
		file.close();

		gs = glCreateShader(GL_GEOMETRY_SHADER);
		gsSource = gsString.c_str();
		glShaderSource(gs, 1, &gsSource, NULL);
		glCompileShader(gs);
		glGetShaderiv(gs, GL_COMPILE_STATUS, &isCompiled);
		glAttachShader(program, gs);

		if (!isCompiled) {
			GLint errorLength;
			string error;
			glGetShaderiv(gs, GL_INFO_LOG_LENGTH, &errorLength);
			error.resize(errorLength);
			glGetShaderInfoLog(gs, errorLength, 0, &error[0]);
			cout << "GEOMETRY SHADER COMPILATION ERROR!" << endl << error << " in " << geometryFilename << endl << endl;
		}
	}

	// Fragment shader
	file.open(fragmentFilename);
	fsString = "";
	while (getline(file, line))
		fsString += line + '\n';
	file.close();

	fs = glCreateShader(GL_FRAGMENT_SHADER);
	fsSource = fsString.c_str();
	glShaderSource(fs, 1, &fsSource, NULL);
	glCompileShader(fs);
	glGetShaderiv(fs, GL_COMPILE_STATUS, &isCompiled);
	glAttachShader(program, fs);

	if (!isCompiled) {
		GLint errorLength;
		string error;
		glGetShaderiv(fs, GL_INFO_LOG_LENGTH, &errorLength);
		error.resize(errorLength);
		glGetShaderInfoLog(fs, errorLength, 0, &error[0]);
		cout << "FRAGMENT SHADER COMPILATION ERROR!" << endl << error << " in " << fragmentFilename << endl << endl;
	}

	// Create shader program
	glLinkProgram(program);
}

void Shader::render3D(Mat4x4 matrix, Object* object, TriangleMesh* mesh) {

	// Start up shader
	glUseProgram(program);
	GLint aPos = glGetAttribLocation(program, "aPos");
	GLint aNormal = glGetAttribLocation(program, "aNormal");
	GLint aTexCoord = glGetAttribLocation(program, "aTexCoord");
	GLint uMat = glGetUniformLocation(program, "uMat");
	GLint uSampler = glGetUniformLocation(program, "uSampler");
	GLint uColor = glGetUniformLocation(program, "uColor");

	// Pass buffers
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vboPos);
	glEnableVertexAttribArray(aPos);
	glVertexAttribPointer(aPos, 3, GL_FLOAT, GL_FALSE, 0, 0);
	
	// Breaks OptiX OpenGL textures
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vboNormal);
	glEnableVertexAttribArray(aNormal);
	glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->vboTexCoord);
	glEnableVertexAttribArray(aTexCoord);
	glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);

	// Pass the viewing transform to the shader
	glUniformMatrix4fv(uMat, 1, GL_FALSE, matrix.e);

	// Send in color
	glUniform4f(uColor, 1.f, 1.f, 1.f, 1.f);

	// Send in texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, mesh->material->image->texture);
	glUniform1i(uSampler, 0);

	// Set up shader specific uniforms
	if (setup)
		setup(program, object, mesh);

	// Draw all triangles
	glEnable(GL_DEPTH_TEST);
	glDrawElements(GL_TRIANGLES, mesh->indexData.size() * 3, GL_UNSIGNED_INT, 0);
	glDisable(GL_DEPTH_TEST);

	// Disable shader
	glDisableVertexAttribArray(aPos);
	glDisableVertexAttribArray(aNormal);
	glDisableVertexAttribArray(aTexCoord);
	glBindTexture(GL_TEXTURE_2D, 0);

}

void Shader::render2D(Mat4x4 matrix, Vec3* posData, Vec2* texCoordData, int vertices, GLuint texture, Color color) {

	// Start up shader
	glUseProgram(program);
	GLint aPos = glGetAttribLocation(program, "aPos");
	GLint aTexCoord = glGetAttribLocation(program, "aTexCoord");
	GLint uMat = glGetUniformLocation(program, "uMat");
	GLint uSampler = glGetUniformLocation(program, "uSampler");
	GLint uColor = glGetUniformLocation(program, "uColor");

	// Select current resources
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Bind buffers
	uint sizePositions = vertices * sizeof(Vec3);
	uint sizeTexCoords = vertices * sizeof(Vec2);
	glBufferData(GL_ARRAY_BUFFER, sizePositions + sizeTexCoords, NULL, GL_DYNAMIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizePositions, posData);
	glBufferSubData(GL_ARRAY_BUFFER, sizePositions, sizeTexCoords, texCoordData);

	// Pass buffers
	glEnableVertexAttribArray(aPos);
	glEnableVertexAttribArray(aTexCoord);
	glVertexAttribPointer(aPos, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, (void*)sizePositions);

	// Send in matrix
	glUniformMatrix4fv(uMat, 1, GL_FALSE, matrix.e);

	// Send in color
	glUniform4fv(uColor, 1, (float*)&color.eCol);

	// Send in texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(uSampler, 0);

	// Draw all triangles
	glDrawArrays(GL_TRIANGLES, 0, vertices);

	// Reset
	glDisableVertexAttribArray(aPos);
	glDisableVertexAttribArray(aTexCoord);
	glBindTexture(GL_TEXTURE_2D, 0);

}

void Shader::render2DBox(Mat4x4 matrix, int x, int y, int width, int height, GLuint texture, Color color) {

	Vec3 posData[6] = {
		{ x, y, 0 },
		{ x, y + height, 0 },
		{ x + width, y, 0 },
		{ x + width, y, 0 },
		{ x, y + height, 0 },
		{ x + width, y + height, 0 },
	};
	Vec2 texCoordData[6] = {
		{ 0, 0 },
		{ 0, 1 },
		{ 1, 0 },
		{ 1, 0 },
		{ 0, 1 },
		{ 1, 1 }
	};

	render2D(matrix, posData, texCoordData, 6, texture, color);
	

}