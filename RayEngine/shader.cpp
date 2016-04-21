#include "shader.h"

Shader::Shader(string name, function<void(GLuint, void*)> setup, string vertexFilename, string fragmentFilename, string geometryFilename) {
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

void Shader::use(Graphic* graphic, Mat4x4 matrix, void* caller) {

	// Start up shader
	glUseProgram(program);
	GLint aPos = glGetAttribLocation(program, "aPos");
	GLint aTexCoord = glGetAttribLocation(program, "aTexCoord");
	GLint aColor = glGetAttribLocation(program, "aColor");
	GLint uMat = glGetUniformLocation(program, "uMat");
	GLint uTex = glGetUniformLocation(program, "uTex");

	// Select current resources
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	// Bind buffers
	uint vertices = graphic->posData.size();
	uint sizePositions = vertices * sizeof(Vec3);
	uint sizeTexCoords = vertices * sizeof(Vec2);
	uint sizeColors = vertices * sizeof(Color);
	glBufferData(GL_ARRAY_BUFFER, sizePositions + sizeTexCoords + sizeColors, NULL, GL_DYNAMIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizePositions, &graphic->posData[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizePositions, sizeTexCoords, &graphic->texCoordData[0]);
	glBufferSubData(GL_ARRAY_BUFFER, sizePositions + sizeTexCoords, sizeColors, &graphic->colorData[0]);

	// Pass buffers
	glEnableVertexAttribArray(aPos);
	glEnableVertexAttribArray(aTexCoord);
	glEnableVertexAttribArray(aColor);
	glVertexAttribPointer(aPos, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, (void*)sizePositions);
	glVertexAttribPointer(aColor, 4, GL_FLOAT, GL_FALSE, 0, (void*)(sizePositions + sizeTexCoords));

	// Send in matrix
	glUniformMatrix4fv(uMat, 1, GL_FALSE, matrix.e);

	// Send in texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, graphic->texture);
	glUniform1i(uTex, 0);

	// Set up shader specific uniforms
	if (setup)
		setup(program, caller);

	// Draw all triangles
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDrawArrays(GL_TRIANGLES, 0, vertices);

}

void Shader::use(Graphic* graphic, Mat4x4 matrix) {
	use(graphic, matrix, this);
}

void Shader::use(TriangleMesh* mesh, Mat4x4 matrix, void* caller) {

	// Start up shader
	glUseProgram(program);
	GLint aPos = glGetAttribLocation(program, "aPos");
	GLint aNorm = glGetAttribLocation(program, "aNorm");
	GLint aTexCoord = glGetAttribLocation(program, "aTexCoord");
	GLint uMat = glGetUniformLocation(program, "uMat");
	GLint uTex = glGetUniformLocation(program, "uTex");

	// Select current resources
	glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);

	// Pass buffers
	uint vertices = mesh->posData.size();
	uint sizePositions = vertices * sizeof(Vec3);
	uint sizeNormals = vertices * sizeof(Vec3);
	glVertexAttribPointer(aPos, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribPointer(aNorm, 3, GL_FLOAT, GL_FALSE, 0, (void*)sizePositions);
	glVertexAttribPointer(aTexCoord, 2, GL_FLOAT, GL_FALSE, 0, (void*)(sizePositions + sizeNormals));

	// Pass the viewing transform to the shader
	glUniformMatrix4fv(uMat, 1, GL_FALSE, matrix.e);

	// Send in texture
	/*glActiveTexture(GL_TEXTURE0);
	if (mesh->material)
		glBindTexture(GL_TEXTURE_2D, mesh->material->texture->texture);
	else
		glBindTexture(GL_TEXTURE_2D, 0);
	glUniform1i(uTex, 0);*/

	// Set up shader specific uniforms
	if (setup)
		setup(program, caller);

	// Draw all triangles
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDrawElements(GL_TRIANGLES, mesh->primitives.size() * 3, GL_UNSIGNED_INT, 0);
}

void Shader::use(TriangleMesh* mesh, Mat4x4 matrix) {
	use(mesh, matrix, this);
}