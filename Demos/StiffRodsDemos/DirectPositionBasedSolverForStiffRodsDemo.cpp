#include "Common/Common.h"
#include "Simulation/CubicSDFCollisionDetection.h"
#include "Simulation/DistanceFieldCollisionDetection.h"
#include "Simulation/TimeManager.h"
#include "Simulation/SimulationModel.h"
#include "Simulation/TimeStepController.h"
#include "Utils/FileSystem.h"
#include "Utils/Logger.h"
#include "Utils/OBJLoader.h"
#include "Utils/TetGenLoader.h"
#include "Utils/Timing.h"
#include "Demos/Visualization/MiniGL.h"
#include "Demos/Visualization/Selection.h"
#include "Demos/Visualization/Visualization.h"
#include "Demos/Common/DemoBase.h"
#include "Demos/Common/TweakBarParameters.h"
#include "Simulation/Simulation.h"
#include <Eigen/Dense>
#include <iostream>
#include <set>

#include "StiffRodsSceneLoader.h"

#define _USE_MATH_DEFINES
#include "math.h"

// Enable memory leak detection
#if defined(_DEBUG) && !defined(EIGEN_ALIGN)
#define new DEBUG_NEW 
#endif

using namespace PBD;
using namespace Eigen;
using namespace std;
using namespace Utilities;

void timeStep();
void buildModel();
void readScene();
void render();
void reset();

void TW_CALL setContactTolerance(const void *value, void *clientData);
void TW_CALL getContactTolerance(void *value, void *clientData);
void TW_CALL setContactStiffnessRigidBody(const void *value, void *clientData);
void TW_CALL getContactStiffnessRigidBody(void *value, void *clientData);
void TW_CALL setContactStiffnessParticleRigidBody(const void *value, void *clientData);
void TW_CALL getContactStiffnessParticleRigidBody(void *value, void *clientData);
void TW_CALL setBendingMethod(const void *value, void *clientData);
void TW_CALL getBendingMethod(void *value, void *clientData);
void TW_CALL setClothSimulationMethod(const void *value, void *clientData);
void TW_CALL getClothSimulationMethod(void *value, void *clientData);
void TW_CALL setSolidSimulationMethod(const void *value, void *clientData);
void TW_CALL getSolidSimulationMethod(void *value, void *clientData);
void TW_CALL setBendingStiffness(const void* value, void* clientData);
void TW_CALL getBendingStiffness(void* value, void* clientData);
void TW_CALL setDistanceStiffness(const void* value, void* clientData);
void TW_CALL getDistanceStiffness(void* value, void* clientData);
void TW_CALL setXXStiffness(const void* value, void* clientData);
void TW_CALL getXXStiffness(void* value, void* clientData);
void TW_CALL setYYStiffness(const void* value, void* clientData);
void TW_CALL getYYStiffness(void* value, void* clientData);
void TW_CALL setXYStiffness(const void* value, void* clientData);
void TW_CALL getXYStiffness(void* value, void* clientData);
void TW_CALL setXYPoissonRatio(const void* value, void* clientData);
void TW_CALL getXYPoissonRatio(void* value, void* clientData);
void TW_CALL setYXPoissonRatio(const void* value, void* clientData);
void TW_CALL getYXPoissonRatio(void* value, void* clientData);
void TW_CALL setNormalizeStretch(const void* value, void* clientData);
void TW_CALL getNormalizeStretch(void* value, void* clientData);
void TW_CALL setNormalizeShear(const void* value, void* clientData);
void TW_CALL getNormalizeShear(void* value, void* clientData);
void TW_CALL setSolidStiffness(const void* value, void* clientData);
void TW_CALL getSolidStiffness(void* value, void* clientData);
void TW_CALL setSolidPoissonRatio(const void* value, void* clientData);
void TW_CALL getSolidPoissonRatio(void* value, void* clientData);
void TW_CALL setVolumeStiffness(const void* value, void* clientData);
void TW_CALL getVolumeStiffness(void* value, void* clientData);
void TW_CALL setSolidNormalizeStretch(const void* value, void* clientData);
void TW_CALL getSolidNormalizeStretch(void* value, void* clientData);
void TW_CALL setSolidNormalizeShear(const void* value, void* clientData);
void TW_CALL getSolidNormalizeShear(void* value, void* clientData);

void singleStep();


DemoBase *base;
CubicSDFCollisionDetection cd;

short clothSimulationMethod = 2;
short solidSimulationMethod = 2;
short bendingMethod = 2;
Real distanceStiffness = 1.0;
Real xxStiffness = 1.0;
Real yyStiffness = 1.0;
Real xyStiffness = 1.0;
Real xyPoissonRatio = 0.3;
Real yxPoissonRatio = 0.3;
bool normalizeStretch = false;
bool normalizeShear = false;
Real bendingStiffness = 0.01;
Real solidStiffness = 1.0;
Real solidPoissonRatio = 0.3;
bool solidNormalizeStretch = false;
bool solidNormalizeShear = false;
Real volumeStiffness = 1.0;

string sceneFileName = "/scenes/Wilberforce_scene.json";
string sceneName;
Vector3r camPos;
Vector3r camLookat;


// main 
int main(int argc, char **argv)
{
	REPORT_MEMORY_LEAKS;
	
	std::string exePath = FileSystem::getProgramPath();

	if (argc > 1)
		sceneFileName = string(argv[1]);
	else
		sceneFileName = FileSystem::normalizePath(exePath + "/resources" + sceneFileName);

	base = new DemoBase();
	base->init(argc, argv, sceneFileName.c_str());
	base->setSceneLoader(new StiffRodsSceneLoader());

	SimulationModel *model = new SimulationModel();
	model->init();
	Simulation::getCurrent()->setModel(model);

	buildModel();

	base->createParameterGUI();
	base->readParameters();

	// OpenGL
	MiniGL::setClientIdleFunc(timeStep);
	MiniGL::addKeyFunc('r', reset);
	MiniGL::addKeyFunc('s', singleStep);
	MiniGL::setClientSceneFunc(render);
	MiniGL::setViewport(40.0f, 0.1f, 500.0f, camPos, camLookat);

	SimulationModel::TriangleModelVector &triModels = model->getTriangleModels();
	SimulationModel::TetModelVector &tetModels = model->getTetModels();
	TwAddVarCB(MiniGL::getTweakBar(), "ContactTolerance", TW_TYPE_REAL, setContactTolerance, getContactTolerance, &cd, " label='Contact tolerance'  min=0.0 step=0.001 precision=3 group=Simulation ");
	TwAddVarCB(MiniGL::getTweakBar(), "ContactStiffnessRigidBody", TW_TYPE_REAL, setContactStiffnessRigidBody, getContactStiffnessRigidBody, &model, " label='Contact stiffness RB'  min=0.0 step=0.1 precision=2 group=Simulation ");
	TwAddVarCB(MiniGL::getTweakBar(), "ContactStiffnessParticleRigidBody", TW_TYPE_REAL, setContactStiffnessParticleRigidBody, getContactStiffnessParticleRigidBody, &model, " label='Contact stiffness Particle-RB'  min=0.0 step=0.1 precision=2 group=Simulation ");
	if (triModels.size() > 0)
	{
		TwType enumType2 = TwDefineEnum("ClothSimulationMethodType", NULL, 0);
		TwAddVarCB(MiniGL::getTweakBar(), "ClothSimulationMethod", enumType2, setClothSimulationMethod, getClothSimulationMethod, &clothSimulationMethod, 
			" label='Cloth sim. method' enum='0 {None}, 1 {Distance constraints}, 2 {FEM based PBD}, 3 {Strain based dynamics}, 4 {XPBD distance constraints}' group=Simulation");
		TwAddVarCB(MiniGL::getTweakBar(), "DistanceStiffness", TW_TYPE_REAL, setDistanceStiffness, getDistanceStiffness, model, " label='Distance constraint stiffness'  min=0.0 step=0.1 precision=4 group='Cloth' ");
		TwAddVarCB(MiniGL::getTweakBar(), "xxStiffness", TW_TYPE_REAL, setXXStiffness, getXXStiffness, model, " label='xx stiffness'  min=0.0 step=0.1 precision=4 group='Cloth' ");
		TwAddVarCB(MiniGL::getTweakBar(), "yyStiffness", TW_TYPE_REAL, setYYStiffness, getYYStiffness, model, " label='yy stiffness'  min=0.0 step=0.1 precision=4 group='Cloth' ");
		TwAddVarCB(MiniGL::getTweakBar(), "xyStiffness", TW_TYPE_REAL, setXYStiffness, getXYStiffness, model, " label='xy stiffness'  min=0.0 step=0.1 precision=4 group='Cloth' ");
		TwAddVarCB(MiniGL::getTweakBar(), "xyPoissonRatio", TW_TYPE_REAL, setXYPoissonRatio, getXYPoissonRatio, model, " label='xy Poisson ratio'  min=0.0 step=0.1 precision=4 group='Cloth' ");
		TwAddVarCB(MiniGL::getTweakBar(), "yxPoissonRatio", TW_TYPE_REAL, setYXPoissonRatio, getYXPoissonRatio, model, " label='yx Poisson ratio'  min=0.0 step=0.1 precision=4 group='Cloth' ");
		TwAddVarCB(MiniGL::getTweakBar(), "normalizeStretch", TW_TYPE_BOOL32, setNormalizeStretch, getNormalizeStretch, model, " label='Normalize stretch' group='Cloth' ");
		TwAddVarCB(MiniGL::getTweakBar(), "normalizeShear", TW_TYPE_BOOL32, setNormalizeShear, getNormalizeShear, model, " label='Normalize shear' group='Cloth' ");
	}
	if (tetModels.size() > 0)
	{
		TwType enumType3 = TwDefineEnum("SolidSimulationMethodType", NULL, 0);
		TwAddVarCB(MiniGL::getTweakBar(), "SolidSimulationMethod", enumType3, setSolidSimulationMethod, getSolidSimulationMethod, &solidSimulationMethod,
			" label='Solid sim. method' enum='0 {None}, 1 {Volume constraints}, 2 {FEM based PBD}, 3 {FEM based XPBD}, \
			4 {Strain based dynamics (no inversion handling)}, 5 {Shape matching (no inversion handling)}, 6 {XPBD volume constraints}' group=Simulation");
		TwAddVarCB(MiniGL::getTweakBar(), "stiffness", TW_TYPE_REAL, setSolidStiffness, getSolidStiffness, model, " label='Stiffness'  min=0.0 step=0.1 precision=4 group='Solid' ");
		TwAddVarCB(MiniGL::getTweakBar(), "poissonRatio", TW_TYPE_REAL, setSolidPoissonRatio, getSolidPoissonRatio, model, " label='Poisson ratio'  min=0.0 step=0.1 precision=4 group='Solid' ");
		TwAddVarCB(MiniGL::getTweakBar(), "normalizeStretch", TW_TYPE_BOOL32, setSolidNormalizeStretch, getSolidNormalizeStretch, model, " label='Normalize stretch' group='Solid' ");
		TwAddVarCB(MiniGL::getTweakBar(), "normalizeShear", TW_TYPE_BOOL32, setSolidNormalizeShear, getSolidNormalizeShear, model, " label='Normalize shear' group='Solid' ");
		TwAddVarCB(MiniGL::getTweakBar(), "volumeStiffness", TW_TYPE_REAL, setVolumeStiffness, getVolumeStiffness, model, " label='Volume stiffness'  min=0.0 step=0.1 precision=4 group='Solid' ");
	}
	if (triModels.size() > 0)
	{
		TwType enumType4 = TwDefineEnum("BendingMethodType", NULL, 0);
		TwAddVarCB(MiniGL::getTweakBar(), "BendingMethod", enumType4, setBendingMethod, getBendingMethod, &bendingMethod, " label='Bending method' enum='0 {None}, 1 {Dihedral angle}, 2 {Isometric bending}, 3 {XPBD isometric bending}' group=Bending");
		TwAddVarCB(MiniGL::getTweakBar(), "BendingStiffness", TW_TYPE_REAL, setBendingStiffness, getBendingStiffness, model, " label='Bending stiffness'  min=0.0 step=0.1 precision=4 group='Bending' ");
	}

	MiniGL::mainLoop();

	base->cleanup();

	Utilities::Timing::printAverageTimes();
	Utilities::Timing::printTimeSums();

	delete Simulation::getCurrent();
	delete base;
	delete model;

	return 0;
}

void reset()
{
	Utilities::Timing::printAverageTimes();
	Utilities::Timing::reset();

	Simulation::getCurrent()->reset();
	base->getSelectedParticles().clear();

	Simulation::getCurrent()->getModel()->cleanup();
	Simulation::getCurrent()->getTimeStep()->getCollisionDetection()->cleanup();

	readScene();
}

void singleStep()
{
	// Simulation code
	SimulationModel *model = Simulation::getCurrent()->getModel();
	Simulation::getCurrent()->getTimeStep()->step(*model);

	// Update visualization models
	const ParticleData &pd = model->getParticles();
	for (unsigned int i = 0; i < model->getTetModels().size(); i++)
	{
		model->getTetModels()[i]->updateMeshNormals(pd);
		model->getTetModels()[i]->updateVisMesh(pd);
	}
	for (unsigned int i = 0; i < model->getTriangleModels().size(); i++)
	{
		model->getTriangleModels()[i]->updateMeshNormals(pd);
	}
}

void timeStep()
{
	const Real pauseAt = base->getValue<Real>(DemoBase::PAUSE_AT);
	if ((pauseAt > 0.0) && (pauseAt < TimeManager::getCurrent()->getTime()))
		base->setValue(DemoBase::PAUSE, true);

	if (base->getValue<bool>(DemoBase::PAUSE))
		return;

	// Simulation code
	SimulationModel *model = Simulation::getCurrent()->getModel();
	const unsigned int numSteps = base->getValue<unsigned int>(DemoBase::NUM_STEPS_PER_RENDER);
	for (unsigned int i = 0; i < numSteps; i++)
	{
		START_TIMING("SimStep");
		Simulation::getCurrent()->getTimeStep()->step(*model);
		STOP_TIMING_AVG;
	}

	// Update visualization models
	const ParticleData &pd = model->getParticles();
	for (unsigned int i = 0; i < model->getTetModels().size(); i++)
	{
		model->getTetModels()[i]->updateMeshNormals(pd);
		model->getTetModels()[i]->updateVisMesh(pd);
	}
	for (unsigned int i = 0; i < model->getTriangleModels().size(); i++)
	{
		model->getTriangleModels()[i]->updateMeshNormals(pd);
	}
}

void render()
{
	base->render();
}

void buildModel()
{
	TimeManager::getCurrent()->setTimeStepSize(static_cast<Real>(0.005));

	SimulationModel *model = Simulation::getCurrent()->getModel();
	Simulation::getCurrent()->getTimeStep()->setCollisionDetection(*model, &cd);

	readScene();
}

void initTriangleModelConstraints()
{
	// init constraints
	SimulationModel *model = Simulation::getCurrent()->getModel();
	for (unsigned int cm = 0; cm < model->getTriangleModels().size(); cm++)
	{
		distanceStiffness = 1.0;
		if (clothSimulationMethod == 4)
			distanceStiffness = 100000;
		model->addClothConstraints(model->getTriangleModels()[cm], clothSimulationMethod, distanceStiffness, xxStiffness,
			yyStiffness, xyStiffness, xyPoissonRatio, yxPoissonRatio, normalizeStretch, normalizeShear);

		bendingStiffness = 0.01;
		if (bendingMethod == 3)
			bendingStiffness = 100.0;
		model->addBendingConstraints(model->getTriangleModels()[cm], bendingMethod, bendingStiffness);
	}
}

void initTetModelConstraints()
{
	// init constraints
	SimulationModel* model = Simulation::getCurrent()->getModel();
	solidStiffness = 1.0;
	if (solidSimulationMethod == 3)
		solidStiffness = 1000000;
	if (solidSimulationMethod == 6)
		solidStiffness = 100000;

	volumeStiffness = 1.0;
	if (solidSimulationMethod == 6)
		volumeStiffness = 100000;
	for (unsigned int cm = 0; cm < model->getTetModels().size(); cm++)
	{
		model->addSolidConstraints(model->getTetModels()[cm], solidSimulationMethod, solidStiffness,
			solidPoissonRatio, volumeStiffness, solidNormalizeStretch, solidNormalizeShear);
	}
}

void loadObj(const std::string &filename, VertexData &vd, IndexedFaceMesh &mesh, const Vector3r &scale)
{
	std::vector<OBJLoader::Vec3f> x;
	std::vector<OBJLoader::Vec3f> normals;
	std::vector<OBJLoader::Vec2f> texCoords;
	std::vector<MeshFaceIndices> faces;
	OBJLoader::Vec3f s = { (float)scale[0], (float)scale[1], (float)scale[2] };
	OBJLoader::loadObj(filename, &x, &faces, &normals, &texCoords, s);

	mesh.release();
	const unsigned int nPoints = (unsigned int)x.size();
	const unsigned int nFaces = (unsigned int)faces.size();
	const unsigned int nTexCoords = (unsigned int)texCoords.size();
	mesh.initMesh(nPoints, nFaces * 2, nFaces);
	vd.reserve(nPoints);
	for (unsigned int i = 0; i < nPoints; i++)
	{
		vd.addVertex(Vector3r(x[i][0], x[i][1], x[i][2]));
	}
	for (unsigned int i = 0; i < nTexCoords; i++)
	{
		mesh.addUV(texCoords[i][0], texCoords[i][1]);
	}
	for (unsigned int i = 0; i < nFaces; i++)
	{
		// Reduce the indices by one
		int posIndices[3];
		int texIndices[3];
		for (int j = 0; j < 3; j++)
		{
			posIndices[j] = faces[i].posIndices[j] - 1;
			if (nTexCoords > 0)
			{
				texIndices[j] = faces[i].texIndices[j] - 1;
				mesh.addUVIndex(texIndices[j]);
			}
		}

		mesh.addFace(&posIndices[0]);
	}
	mesh.buildNeighbors();

	mesh.updateNormals(vd, 0);
	mesh.updateVertexNormals(vd);

	LOG_INFO << "Number of triangles: " << nFaces;
	LOG_INFO << "Number of vertices: " << nPoints;
}

CubicSDFCollisionDetection::GridPtr generateSDF(const std::string &modelFile, const std::string &collisionObjectFileName, const Eigen::Matrix<unsigned int, 3, 1> &resolutionSDF,
	VertexData &vd, IndexedFaceMesh &mesh)
{
	const std::string basePath = FileSystem::getFilePath(base->getSceneFile());
	const string cachePath = basePath + "/Cache";
	const std::string modelFileName = FileSystem::getFileNameWithExt(modelFile);
	CubicSDFCollisionDetection::GridPtr distanceField;

	if (collisionObjectFileName == "")
	{
		std::string md5FileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + ".md5");
		string md5Str = FileSystem::getFileMD5(modelFile);
		bool md5 = false;
		if (FileSystem::fileExists(md5FileName))
			md5 = FileSystem::checkMD5(md5Str, md5FileName);

		// check MD5 if cache file is available
		const string resStr = to_string(resolutionSDF[0]) + "_" + to_string(resolutionSDF[1]) + "_" + to_string(resolutionSDF[2]);
		const string sdfFileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + "_" + resStr + ".csdf");
		bool foundCacheFile = FileSystem::fileExists(sdfFileName);

		if (foundCacheFile && md5)
		{
			LOG_INFO << "Load cached SDF: " << sdfFileName;
			distanceField = std::make_shared<CubicSDFCollisionDetection::Grid>(sdfFileName);
		}
		else
		{
			std::vector<unsigned int> &faces = mesh.getFaces();
			const unsigned int nFaces = mesh.numFaces();
#ifdef USE_DOUBLE
			Discregrid::TriangleMesh sdfMesh(&vd.getPosition(0)[0], faces.data(), vd.size(), nFaces);
#else
			// if type is float, copy vector to double vector
			std::vector<double> doubleVec;
			doubleVec.resize(3 * vd.size());
			for (unsigned int i = 0; i < vd.size(); i++)
				for (unsigned int j = 0; j < 3; j++)
					doubleVec[3 * i + j] = vd.getPosition(i)[j];
			Discregrid::TriangleMesh sdfMesh(&doubleVec[0], faces.data(), vd.size(), nFaces);
#endif
			Discregrid::TriangleMeshDistance md(sdfMesh);
			Eigen::AlignedBox3d domain;
			for (auto const& x : sdfMesh.vertices())
			{
				domain.extend(x);
			}
			domain.max() += 0.1 * Eigen::Vector3d::Ones();
			domain.min() -= 0.1 * Eigen::Vector3d::Ones();

			LOG_INFO << "Set SDF resolution: " << resolutionSDF[0] << ", " << resolutionSDF[1] << ", " << resolutionSDF[2];
			distanceField = std::make_shared<CubicSDFCollisionDetection::Grid>(domain, std::array<unsigned int, 3>({ resolutionSDF[0], resolutionSDF[1], resolutionSDF[2] }));
			auto func = Discregrid::DiscreteGrid::ContinuousFunction{};
			func = [&md](Eigen::Vector3d const& xi) {return md.signed_distance(xi).distance; };
			LOG_INFO << "Generate SDF for " << modelFile;
			distanceField->addFunction(func, true);
			if (FileSystem::makeDir(cachePath) == 0)
			{
				LOG_INFO << "Save SDF: " << sdfFileName;
				distanceField->save(sdfFileName);
				FileSystem::writeMD5File(modelFile, md5FileName);
			}
		}
	}
	else
	{
		std::string fileName = collisionObjectFileName;
		if (FileSystem::isRelativePath(fileName))
		{
			fileName = FileSystem::normalizePath(basePath + "/" + fileName);
		}
		LOG_INFO << "Load SDF: " << fileName;
		distanceField = std::make_shared<CubicSDFCollisionDetection::Grid>(fileName);
	}
	return distanceField;
}


/** Create the rigid body model
*/
void readScene()
{
	SimulationModel *model = Simulation::getCurrent()->getModel();
	SimulationModel::RigidBodyVector &rb = model->getRigidBodies();
	SimulationModel::TriangleModelVector &triModels = model->getTriangleModels();
	SimulationModel::TetModelVector &tetModels = model->getTetModels();
	SimulationModel::ConstraintVector &constraints = model->getConstraints();

	StiffRodsSceneLoader::RodSceneData data;
	StiffRodsSceneLoader *loader = static_cast<StiffRodsSceneLoader*>(base->getSceneLoader());
	loader->readStiffRodsScene(sceneFileName, data);
	LOG_INFO << "Scene: " << sceneFileName;

	camPos = data.m_camPosition;
	camLookat = data.m_camLookat;

	TimeManager::getCurrent()->setTimeStepSize(data.m_timeStepSize);

	sceneName = data.m_sceneName;

	if (data.m_triangleModelSimulationMethod != -1)
		clothSimulationMethod = data.m_triangleModelSimulationMethod;
	if (data.m_tetModelSimulationMethod != -1)
		solidSimulationMethod = data.m_tetModelSimulationMethod;
	if (data.m_triangleModelBendingMethod != -1)
		bendingMethod = data.m_triangleModelBendingMethod;
	cd.setTolerance(data.m_contactTolerance);
	model->setContactStiffnessRigidBody(data.m_contactStiffnessRigidBody);
	model->setContactStiffnessParticleRigidBody(data.m_contactStiffnessParticleRigidBody);


	bendingStiffness = data.m_cloth_bendingStiffness;
	xxStiffness = data.m_cloth_xxStiffness;
	yyStiffness = data.m_cloth_yyStiffness;
	xyStiffness = data.m_cloth_xyStiffness;
	xyPoissonRatio = data.m_cloth_xyPoissonRatio;
	yxPoissonRatio = data.m_cloth_yxPoissonRatio;
	normalizeStretch = data.m_cloth_normalizeStretch;
	normalizeShear = data.m_cloth_normalizeShear;

	solidStiffness = data.m_solid_stiffness;
	solidPoissonRatio = data.m_solid_poissonRatio;
	solidNormalizeStretch = data.m_solid_normalizeStretch;
	solidNormalizeShear = data.m_solid_normalizeShear;
	volumeStiffness = data.m_volume_stiffness;

	//////////////////////////////////////////////////////////////////////////
	// rigid bodies
	//////////////////////////////////////////////////////////////////////////

	// map file names to loaded geometry to prevent multiple imports of same files
	std::map<std::string, pair<VertexData, IndexedFaceMesh>> objFiles;
	std::map<std::string, CubicSDFCollisionDetection::GridPtr> distanceFields;
	for (unsigned int rbIndex = 0; rbIndex < data.m_rigidBodyData.size(); rbIndex++)
	{
		SceneLoader::RigidBodyData &rbd = data.m_rigidBodyData[rbIndex];

		// Check if already loaded
		rbd.m_modelFile = FileSystem::normalizePath(rbd.m_modelFile);
		if (objFiles.find(rbd.m_modelFile) == objFiles.end())
		{
			IndexedFaceMesh mesh;
			VertexData vd;
			loadObj(rbd.m_modelFile, vd, mesh, Vector3r::Ones());
			objFiles[rbd.m_modelFile] = { vd, mesh };
		}

		const std::string basePath = FileSystem::getFilePath(sceneFileName);
		const string cachePath = basePath + "/Cache";
		const string resStr = to_string(rbd.m_resolutionSDF[0]) + "_" + to_string(rbd.m_resolutionSDF[1]) + "_" + to_string(rbd.m_resolutionSDF[2]);
		const std::string modelFileName = FileSystem::getFileNameWithExt(rbd.m_modelFile);
		const string sdfFileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + "_" + resStr + ".csdf");

		std::string sdfKey = rbd.m_collisionObjectFileName;
		if (sdfKey == "")
		{
			sdfKey = sdfFileName;
		}
		if (distanceFields.find(sdfKey) == distanceFields.end())
		{
			// Generate SDF
			if (rbd.m_collisionObjectType == SceneLoader::SDF)
			{
				if (rbd.m_collisionObjectFileName == "")
				{
					std::string md5FileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + ".md5");
					string md5Str = FileSystem::getFileMD5(rbd.m_modelFile);
					bool md5 = false;
					if (FileSystem::fileExists(md5FileName))
						md5 = FileSystem::checkMD5(md5Str, md5FileName);

					// check MD5 if cache file is available
					const string resStr = to_string(rbd.m_resolutionSDF[0]) + "_" + to_string(rbd.m_resolutionSDF[1]) + "_" + to_string(rbd.m_resolutionSDF[2]);
					const string sdfFileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + "_" + resStr + ".csdf");
					bool foundCacheFile = FileSystem::fileExists(sdfFileName);

					if (foundCacheFile && md5)
					{
						LOG_INFO << "Load cached SDF: " << sdfFileName;
						distanceFields[sdfFileName] = std::make_shared<CubicSDFCollisionDetection::Grid>(sdfFileName);
					}
					else
					{
						VertexData &vd = objFiles[rbd.m_modelFile].first;
						IndexedFaceMesh &mesh = objFiles[rbd.m_modelFile].second;

						std::vector<unsigned int> &faces = mesh.getFaces();
						const unsigned int nFaces = mesh.numFaces();

#ifdef USE_DOUBLE
						Discregrid::TriangleMesh sdfMesh(&vd.getPosition(0)[0], faces.data(), vd.size(), nFaces);
#else
						// if type is float, copy vector to double vector
						std::vector<double> doubleVec;
						doubleVec.resize(3 * vd.size());
						for (unsigned int i = 0; i < vd.size(); i++)
							for (unsigned int j = 0; j < 3; j++)
								doubleVec[3 * i + j] = vd.getPosition(i)[j];
						Discregrid::TriangleMesh sdfMesh(&doubleVec[0], faces.data(), vd.size(), nFaces);
#endif
						Discregrid::TriangleMeshDistance md(sdfMesh);
						Eigen::AlignedBox3d domain;
						for (auto const& x : sdfMesh.vertices())
						{
							domain.extend(x);
						}
						domain.max() += 0.1 * Eigen::Vector3d::Ones();
						domain.min() -= 0.1 * Eigen::Vector3d::Ones();

						LOG_INFO << "Set SDF resolution: " << rbd.m_resolutionSDF[0] << ", " << rbd.m_resolutionSDF[1] << ", " << rbd.m_resolutionSDF[2];
						distanceFields[sdfFileName] = std::make_shared<CubicSDFCollisionDetection::Grid>(domain, std::array<unsigned int, 3>({ rbd.m_resolutionSDF[0], rbd.m_resolutionSDF[1], rbd.m_resolutionSDF[2] }));
						auto func = Discregrid::DiscreteGrid::ContinuousFunction{};
						func = [&md](Eigen::Vector3d const& xi) {return md.signed_distance(xi).distance; };
						LOG_INFO << "Generate SDF for " << rbd.m_modelFile;
						distanceFields[sdfFileName]->addFunction(func, true);
						if (FileSystem::makeDir(cachePath) == 0)
						{
							LOG_INFO << "Save SDF: " << sdfFileName;
							distanceFields[sdfFileName]->save(sdfFileName);
							FileSystem::writeMD5File(rbd.m_modelFile, md5FileName);
						}
					}
				}
				else
				{
					std::string fileName = rbd.m_collisionObjectFileName;
					if (FileSystem::isRelativePath(fileName))
					{
						fileName = FileSystem::normalizePath(basePath + "/" + fileName);
					}
					LOG_INFO << "Load SDF: " << fileName;
					distanceFields[rbd.m_collisionObjectFileName] = std::make_shared<CubicSDFCollisionDetection::Grid>(fileName);
				}
			}
		}
	}

	for (unsigned int rbIndex = 0; rbIndex < data.m_tetModelData.size(); rbIndex++)
	{
		const SceneLoader::TetModelData &tmd = data.m_tetModelData[rbIndex];

		// Check if already loaded
		if ((tmd.m_modelFileVis != "") &&
			(objFiles.find(tmd.m_modelFileVis) == objFiles.end()))
		{
			IndexedFaceMesh mesh;
			VertexData vd;
			loadObj(FileSystem::normalizePath(tmd.m_modelFileVis), vd, mesh, Vector3r::Ones());
			objFiles[tmd.m_modelFileVis] = { vd, mesh };
		}
	}

	// Stiff rods: gather rigid body id of bodies belonging to a tree
	std::set<unsigned int> treeSegments;
	for (auto &tree : data.m_TreeModels)
	{
		treeSegments.insert(tree.m_SegmentIds.begin(), tree.m_SegmentIds.end());
	}
	// end stiff rods

	for (unsigned int i = 0; i < data.m_tetModelData.size(); i++)
	{
		const SceneLoader::TetModelData &tmd = data.m_tetModelData[i];

		// Check if already loaded
		if ((tmd.m_modelFileVis != "") &&
			(objFiles.find(tmd.m_modelFileVis) == objFiles.end()))
		{
			IndexedFaceMesh mesh;
			VertexData vd;
			loadObj(FileSystem::normalizePath(tmd.m_modelFileVis), vd, mesh, Vector3r::Ones());
			objFiles[tmd.m_modelFileVis] = { vd, mesh };
		}

		const std::string basePath = FileSystem::getFilePath(base->getSceneFile());
		const string cachePath = basePath + "/Cache";
		const string resStr = to_string(tmd.m_resolutionSDF[0]) + "_" + to_string(tmd.m_resolutionSDF[1]) + "_" + to_string(tmd.m_resolutionSDF[2]);
		const std::string modelFileName = FileSystem::getFileNameWithExt(tmd.m_modelFileVis);
		const string sdfFileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + "_" + resStr + ".csdf");

		std::string sdfKey = tmd.m_collisionObjectFileName;
		if (sdfKey == "")
		{
			sdfKey = sdfFileName;
		}
		if (distanceFields.find(sdfKey) == distanceFields.end())
		{
			// Generate SDF
			if (tmd.m_collisionObjectType == SceneLoader::SDF)
			{
				VertexData &vd = objFiles[tmd.m_modelFileVis].first;
				IndexedFaceMesh &mesh = objFiles[tmd.m_modelFileVis].second;

				CubicSDFCollisionDetection::GridPtr distanceField = generateSDF(tmd.m_modelFileVis, tmd.m_collisionObjectFileName, tmd.m_resolutionSDF, vd, mesh);

				if (tmd.m_collisionObjectFileName == "")
					distanceFields[sdfFileName] = distanceField;
				else
					distanceFields[tmd.m_collisionObjectFileName] = distanceField;
			}
		}
	}

	rb.resize(data.m_rigidBodyData.size());
	std::map<unsigned int, unsigned int> id_index;
	unsigned int rbIndex = 0;
	for (unsigned int idx(0); idx < data.m_rigidBodyData.size(); ++idx)
	{
		const SceneLoader::RigidBodyData &rbd = data.m_rigidBodyData[idx];

		// Stiff rods: do not handle segments of rods here
		if (treeSegments.find(rbd.m_id) != treeSegments.end())
			continue;
		// end stiff rods

		if (objFiles.find(rbd.m_modelFile) == objFiles.end())
			continue;

		id_index[rbd.m_id] = rbIndex;

		VertexData &vd = objFiles[rbd.m_modelFile].first;
		IndexedFaceMesh &mesh = objFiles[rbd.m_modelFile].second;
		mesh.setFlatShading(rbd.m_flatShading);

		rb[rbIndex] = new RigidBody();
		rb[rbIndex]->initBody(rbd.m_density,
			rbd.m_x,
			rbd.m_q,
			vd, mesh,
			rbd.m_scale);

		if (!rbd.m_isDynamic)
			rb[rbIndex]->setMass(0.0);
		else
		{
			rb[rbIndex]->setVelocity(rbd.m_v);
			rb[rbIndex]->setAngularVelocity(rbd.m_omega);
		}
		rb[rbIndex]->setRestitutionCoeff(rbd.m_restitutionCoeff);
		rb[rbIndex]->setFrictionCoeff(rbd.m_frictionCoeff);

		const std::vector<Vector3r> &vertices = rb[rbIndex]->getGeometry().getVertexDataLocal().getVertices();
		const unsigned int nVert = static_cast<unsigned int>(vertices.size());

		switch (rbd.m_collisionObjectType)
		{
		case SceneLoader::No_Collision_Object: break;
		case SceneLoader::Sphere:
			cd.addCollisionSphere(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, rbd.m_collisionObjectScale[0], rbd.m_testMesh, rbd.m_invertSDF);
			break;
		case SceneLoader::Box:
			cd.addCollisionBox(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, rbd.m_collisionObjectScale, rbd.m_testMesh, rbd.m_invertSDF);
			break;
		case SceneLoader::Cylinder:
			cd.addCollisionCylinder(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, rbd.m_collisionObjectScale.head<2>(), rbd.m_testMesh, rbd.m_invertSDF);
			break;
		case SceneLoader::Torus:
			cd.addCollisionTorus(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, rbd.m_collisionObjectScale.head<2>(), rbd.m_testMesh, rbd.m_invertSDF);
			break;
		case SceneLoader::HollowSphere:
			cd.addCollisionHollowSphere(rbIndex, PBD::CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, rbd.m_collisionObjectScale[0], rbd.m_thicknessSDF, rbd.m_testMesh, rbd.m_invertSDF);
			break;
		case SceneLoader::HollowBox:
			cd.addCollisionHollowBox(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, rbd.m_collisionObjectScale, rbd.m_thicknessSDF, rbd.m_testMesh, rbd.m_invertSDF);
			break;
		case SceneLoader::SDF:
		{
			if (rbd.m_collisionObjectFileName == "")
			{
				const std::string basePath = FileSystem::getFilePath(sceneFileName);
				const string cachePath = basePath + "/Cache";
				const string resStr = to_string(rbd.m_resolutionSDF[0]) + "_" + to_string(rbd.m_resolutionSDF[1]) + "_" + to_string(rbd.m_resolutionSDF[2]);
				const std::string modelFileName = FileSystem::getFileNameWithExt(rbd.m_modelFile);
				const string sdfFileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + "_" + resStr + ".csdf");
				cd.addCubicSDFCollisionObject(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, distanceFields[sdfFileName], rbd.m_collisionObjectScale, rbd.m_testMesh, rbd.m_invertSDF);
			}
			else
				cd.addCubicSDFCollisionObject(rbIndex, CollisionDetection::CollisionObject::RigidBodyCollisionObjectType, vertices.data(), nVert, distanceFields[rbd.m_collisionObjectFileName], rbd.m_collisionObjectScale, rbd.m_testMesh, rbd.m_invertSDF);
			break;
		}
		}
		// stiff rods
		++rbIndex;
		// end stiff rods
	}

	//////////////////////////////////////////////////////////////////////////
	// stiff rod models
	//////////////////////////////////////////////////////////////////////////

	if (data.m_TreeModels.size())
		LOG_INFO << "Loading " << data.m_TreeModels.size() << " stiff rods" ;

	std::map<unsigned int, unsigned int> index_id;

	std::map<unsigned int, StiffRodsSceneLoader::StretchBendingTwistingConstraintData*> cIDTocData;
	for (auto &constraint : data.m_StretchBendingTwistingConstraints)
	{
		cIDTocData[constraint.m_Id] = &constraint;
	}

	unsigned int lengthIdx(1), widthIdx(0), heightIdx(2); // y-axis is used as the length axis to be compatible to Blender's armatures (Blender 2.78)
	std::map<unsigned int, Vector3r> segmentScale;
	std::map<unsigned int, Real> segmentDensity;

	for (auto &treeModel : data.m_TreeModels)
	{
		START_TIMING("Load tree");

		std::vector<RigidBody*> treeSegments;
		treeSegments.reserve(treeModel.m_SegmentIds.size());
		
		const Vector3r &modelPosition(treeModel.m_X);
		const Quaternionr &modelRotationQuat(treeModel.m_Q);
		Matrix3r modelRotationMat(modelRotationQuat.toRotationMatrix());

		std::cout << "Loading " << treeModel.m_SegmentIds.size() << " stiff rod segments" << std::endl;
		for (size_t idx(0); idx < data.m_rigidBodyData.size(); ++idx)
		{
			auto &segmentData(data.m_rigidBodyData[static_cast<unsigned int>(idx)]);
			if (treeModel.m_SegmentIds.find(segmentData.m_id) == treeModel.m_SegmentIds.end())
				continue;

			if (objFiles.find(segmentData.m_modelFile) == objFiles.end())
				continue;

			// continue if segment has already been created because it is also a member of another tree
			if (id_index.find(segmentData.m_id) != id_index.end())
				continue;

			VertexData &vd = objFiles[segmentData.m_modelFile].first;
			IndexedFaceMesh &mesh = objFiles[segmentData.m_modelFile].second;
			mesh.setFlatShading(segmentData.m_flatShading);

			//compute mass and inertia of a cylinder
			Vector3r & scale(segmentData.m_scale);
			Real length(scale[lengthIdx]), width(scale[widthIdx]), height(scale[heightIdx]);
			Real density(segmentData.m_density);
			segmentDensity[segmentData.m_id] = density;

			Real mass(static_cast<Real>(M_PI_4) * width * height * length * density);

			const Real Iy = static_cast<Real>(1. / 12.) * mass*(static_cast<Real>(3.)*(static_cast<Real>(0.25)*height*height) + length*length);
			const Real Iz = static_cast<Real>(1. / 12.) * mass*(static_cast<Real>(3.)*(static_cast<Real>(0.25)*width*width) + length*length);
			const Real Ix = static_cast<Real>(0.25) * mass*(static_cast<Real>(0.25)*(height*height + width*width));

			Vector3r inertiaTensor;
			inertiaTensor(lengthIdx) = Ix;
			inertiaTensor(widthIdx) = Iy;
			inertiaTensor(heightIdx) = Iz;

			rb[rbIndex] = new RigidBody();
			rb[rbIndex]->initBody(mass,
				modelPosition + modelRotationMat * segmentData.m_x,
				inertiaTensor,
				modelRotationQuat * segmentData.m_q,
				vd, mesh, scale);

			if (!segmentData.m_isDynamic)
				rb[rbIndex]->setMass(0.0);
			else
			{
				rb[rbIndex]->setVelocity(segmentData.m_v);
				rb[rbIndex]->setAngularVelocity(segmentData.m_omega);
			}
			rb[rbIndex]->setRestitutionCoeff(segmentData.m_restitutionCoeff);
			rb[rbIndex]->setFrictionCoeff(segmentData.m_frictionCoeff);
			
			segmentScale[segmentData.m_id] = scale;

			id_index[segmentData.m_id] = rbIndex;
			index_id[rbIndex] = segmentData.m_id;
			treeSegments.push_back(rb[rbIndex]);

			++rbIndex;
		}

		// Make roots static;
		for (auto id : treeModel.m_StaticSegments)
		{
			std::map<unsigned int, unsigned int>::iterator it(id_index.find(id));
			if (it != id_index.end())
			{
				rb[it->second]->setMass(0.);
			}
		}


		// create zero-stretch, bending and twisting constraints

		std::vector<std::pair<unsigned int, unsigned int>>  constraintSegmentIndices;
		std::vector<Vector3r> constraintPositions;
		std::vector<Real> averageRadii;
		std::vector<Real> averageSegmentLengths;
		std::vector<Real> youngsModuli(treeModel.m_StretchBendingTwistingConstraintIds.size(), treeModel.m_YoungsModulus);
		std::vector<Real> torsionModuli(treeModel.m_StretchBendingTwistingConstraintIds.size(), treeModel.m_TorsionModulus);

		constraintSegmentIndices.reserve(treeModel.m_StretchBendingTwistingConstraintIds.size());
		constraintPositions.reserve(treeModel.m_StretchBendingTwistingConstraintIds.size());
		averageRadii.reserve(treeModel.m_StretchBendingTwistingConstraintIds.size());
		averageSegmentLengths.reserve(treeModel.m_StretchBendingTwistingConstraintIds.size());

		for (unsigned int cID : treeModel.m_StretchBendingTwistingConstraintIds)
		{
			if (cIDTocData.find(cID) == cIDTocData.end())
				continue;

			auto &sbtConstraint = *cIDTocData[cID];

			std::map<unsigned int, unsigned int>::iterator itFirstIdx(id_index.find(sbtConstraint.m_SegmentID[0])),
				itSecondIdx(id_index.find(sbtConstraint.m_SegmentID[1]));

			if (itFirstIdx == id_index.end() || itSecondIdx == id_index.end())
				continue;

			constraintSegmentIndices.push_back(std::pair<unsigned int, unsigned int>(itFirstIdx->second, itSecondIdx->second));			

			const Vector3r &localPosition(sbtConstraint.m_Position);
			Vector3r x(modelPosition + modelRotationMat * localPosition);
			constraintPositions.push_back(x);

			// compute average length
			Real avgLength(static_cast<Real>(0.5)*(segmentScale[sbtConstraint.m_SegmentID[0]][lengthIdx] + segmentScale[sbtConstraint.m_SegmentID[1]][lengthIdx]));
			averageSegmentLengths.push_back(avgLength);
			
			// compute average radius
			averageRadii.push_back(static_cast<Real>(0.125)*(segmentScale[sbtConstraint.m_SegmentID[0]][widthIdx]
				+ segmentScale[sbtConstraint.m_SegmentID[0]][heightIdx]
				+ segmentScale[sbtConstraint.m_SegmentID[1]][widthIdx] + segmentScale[sbtConstraint.m_SegmentID[1]][heightIdx]));
		}

		model->addDirectPositionBasedSolverForStiffRodsConstraint(constraintSegmentIndices,
			constraintPositions, averageRadii, averageSegmentLengths, youngsModuli, torsionModuli);		

		STOP_TIMING_PRINT;// load tree
	}


	//////////////////////////////////////////////////////////////////////////
	// triangle models
	//////////////////////////////////////////////////////////////////////////

	// map file names to loaded geometry to prevent multiple imports of same files
	std::map<std::string, pair<VertexData, IndexedFaceMesh>> triFiles;
	for (unsigned int i = 0; i < data.m_triangleModelData.size(); i++)
	{
		const SceneLoader::TriangleModelData &tmd = data.m_triangleModelData[i];

		// Check if already loaded
		if (triFiles.find(tmd.m_modelFile) == triFiles.end())
		{
			IndexedFaceMesh mesh;
			VertexData vd;
			loadObj(FileSystem::normalizePath(tmd.m_modelFile), vd, mesh, Vector3r::Ones());
			triFiles[tmd.m_modelFile] = { vd, mesh };
		}
	}

	triModels.reserve(data.m_triangleModelData.size());
	std::map<unsigned int, unsigned int> tm_id_index;
	for (unsigned int i = 0; i < data.m_triangleModelData.size(); i++)
	{
		const SceneLoader::TriangleModelData &tmd = data.m_triangleModelData[i];

		if (triFiles.find(tmd.m_modelFile) == triFiles.end())
			continue;

		tm_id_index[tmd.m_id] = i;

		VertexData vd = triFiles[tmd.m_modelFile].first;
		IndexedFaceMesh &mesh = triFiles[tmd.m_modelFile].second;

		const Matrix3r R = tmd.m_q.matrix();
		for (unsigned int j = 0; j < vd.size(); j++)
		{
			vd.getPosition(j) = R * (vd.getPosition(j).cwiseProduct(tmd.m_scale)) + tmd.m_x;
		}

		model->addTriangleModel(vd.size(), mesh.numFaces(), &vd.getPosition(0), mesh.getFaces().data(), mesh.getUVIndices(), mesh.getUVs());

		TriangleModel *tm = triModels[triModels.size() - 1];
		ParticleData &pd = model->getParticles();
		unsigned int offset = tm->getIndexOffset();

		for (unsigned int j = 0; j < tmd.m_staticParticles.size(); j++)
		{
			const unsigned int index = tmd.m_staticParticles[j] + offset;
			pd.setMass(index, 0.0);
		}

		tm->setRestitutionCoeff(tmd.m_restitutionCoeff);
		tm->setFrictionCoeff(tmd.m_frictionCoeff);
	}

	initTriangleModelConstraints();

	//////////////////////////////////////////////////////////////////////////
	// tet models
	//////////////////////////////////////////////////////////////////////////

	// map file names to loaded geometry to prevent multiple imports of same files
	std::map<pair<string, string>, pair<vector<Vector3r>, vector<unsigned int>>> tetFiles;
	for (unsigned int i = 0; i < data.m_tetModelData.size(); i++)
	{
		const SceneLoader::TetModelData &tmd = data.m_tetModelData[i];

		// Check if already loaded
		pair<string, string> fileNames = { tmd.m_modelFileNodes, tmd.m_modelFileElements };
		if (tetFiles.find(fileNames) == tetFiles.end())
		{
			vector<Vector3r> vertices;
			vector<unsigned int> tets;
			TetGenLoader::loadTetgenModel(FileSystem::normalizePath(tmd.m_modelFileNodes), FileSystem::normalizePath(tmd.m_modelFileElements), vertices, tets);
			tetFiles[fileNames] = { vertices, tets };
		}
	}

	tetModels.reserve(data.m_tetModelData.size());
	std::map<unsigned int, unsigned int> tm_id_index2;
	for (unsigned int i = 0; i < data.m_tetModelData.size(); i++)
	{
		const SceneLoader::TetModelData &tmd = data.m_tetModelData[i];

		pair<string, string> fileNames = { tmd.m_modelFileNodes, tmd.m_modelFileElements };
		auto geo = tetFiles.find(fileNames);
		if (geo == tetFiles.end())
			continue;

		tm_id_index2[tmd.m_id] = i;

		vector<Vector3r> vertices = geo->second.first;
		vector<unsigned int> &tets = geo->second.second;

		const Matrix3r R = tmd.m_q.matrix();
		for (unsigned int j = 0; j < vertices.size(); j++)
		{
			vertices[j] = R * (vertices[j].cwiseProduct(tmd.m_scale)) + tmd.m_x;
		}

		model->addTetModel((unsigned int)vertices.size(), (unsigned int)tets.size() / 4, vertices.data(), tets.data());

		TetModel *tm = tetModels[tetModels.size() - 1];
		ParticleData &pd = model->getParticles();
		unsigned int offset = tm->getIndexOffset();

		tm->setInitialX(tmd.m_x);
		tm->setInitialR(R);
		tm->setInitialScale(tmd.m_scale);

		for (unsigned int j = 0; j < tmd.m_staticParticles.size(); j++)
		{
			const unsigned int index = tmd.m_staticParticles[j] + offset;
			pd.setMass(index, 0.0);
		}

		// read visualization mesh
		if (tmd.m_modelFileVis != "")
		{
			if (objFiles.find(tmd.m_modelFileVis) != objFiles.end())
			{
				IndexedFaceMesh &visMesh = tm->getVisMesh();
				VertexData &vdVis = tm->getVisVertices();
				vdVis = objFiles[tmd.m_modelFileVis].first;
				visMesh = objFiles[tmd.m_modelFileVis].second;

				for (unsigned int j = 0; j < vdVis.size(); j++)
					vdVis.getPosition(j) = R * (vdVis.getPosition(j).cwiseProduct(tmd.m_scale)) + tmd.m_x;

				tm->updateMeshNormals(pd);
				tm->attachVisMesh(pd);
				tm->updateVisMesh(pd);
			}
		}

		tm->setRestitutionCoeff(tmd.m_restitutionCoeff);
		tm->setFrictionCoeff(tmd.m_frictionCoeff);

		tm->updateMeshNormals(pd);
	}

	initTetModelConstraints();

	// init collision objects for deformable models
	ParticleData &pd = model->getParticles();
	for (unsigned int i = 0; i < data.m_triangleModelData.size(); i++)
	{
		TriangleModel *tm = triModels[i];
		unsigned int offset = tm->getIndexOffset();
		const unsigned int nVert = tm->getParticleMesh().numVertices();
		cd.addCollisionObjectWithoutGeometry(i, CollisionDetection::CollisionObject::TriangleModelCollisionObjectType, &pd.getPosition(offset), nVert, true);

	}
	for (unsigned int i = 0; i < data.m_tetModelData.size(); i++)
	{
		TetModel *tm = tetModels[i];
		unsigned int offset = tm->getIndexOffset();
		const unsigned int nVert = tm->getParticleMesh().numVertices();
		const IndexedTetMesh &tetMesh = tm->getParticleMesh();

		const SceneLoader::TetModelData &tmd = data.m_tetModelData[i];

		switch (tmd.m_collisionObjectType)
		{
		case SceneLoader::No_Collision_Object:
			cd.addCollisionObjectWithoutGeometry(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, true);
			break;
		case SceneLoader::Sphere:
			cd.addCollisionSphere(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, tmd.m_collisionObjectScale[0], tmd.m_testMesh, tmd.m_invertSDF);
			break;
		case SceneLoader::Box:
			cd.addCollisionBox(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, tmd.m_collisionObjectScale, tmd.m_testMesh, tmd.m_invertSDF);
			break;
		case SceneLoader::Cylinder:
			cd.addCollisionCylinder(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, tmd.m_collisionObjectScale.head<2>(), tmd.m_testMesh, tmd.m_invertSDF);
			break;
		case SceneLoader::Torus:
			cd.addCollisionTorus(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, tmd.m_collisionObjectScale.head<2>(), tmd.m_testMesh, tmd.m_invertSDF);
			break;
		case SceneLoader::HollowSphere:
			cd.addCollisionHollowSphere(i, PBD::CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, tmd.m_collisionObjectScale[0], tmd.m_thicknessSDF, tmd.m_testMesh, tmd.m_invertSDF);
			break;
		case SceneLoader::HollowBox:
			cd.addCollisionHollowBox(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, tmd.m_collisionObjectScale, tmd.m_thicknessSDF, tmd.m_testMesh, tmd.m_invertSDF);
			break;
		case SceneLoader::SDF:
		{
			if (tmd.m_collisionObjectFileName == "")
			{
				const std::string basePath = FileSystem::getFilePath(base->getSceneFile());
				const string cachePath = basePath + "/Cache";
				const string resStr = to_string(tmd.m_resolutionSDF[0]) + "_" + to_string(tmd.m_resolutionSDF[1]) + "_" + to_string(tmd.m_resolutionSDF[2]);
				const std::string modelFileName = FileSystem::getFileNameWithExt(tmd.m_modelFileVis);
				const string sdfFileName = FileSystem::normalizePath(cachePath + "/" + modelFileName + "_" + resStr + ".csdf");
				cd.addCubicSDFCollisionObject(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, distanceFields[sdfFileName], tmd.m_collisionObjectScale, tmd.m_testMesh, tmd.m_invertSDF);
			}
			else
				cd.addCubicSDFCollisionObject(i, CollisionDetection::CollisionObject::TetModelCollisionObjectType, &pd.getPosition(offset), nVert, distanceFields[tmd.m_collisionObjectFileName], tmd.m_collisionObjectScale, tmd.m_testMesh, tmd.m_invertSDF);
			break;
		}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// joints
	//////////////////////////////////////////////////////////////////////////

	for (unsigned int i = 0; i < data.m_ballJointData.size(); i++)
	{
		const SceneLoader::BallJointData &jd = data.m_ballJointData[i];
		model->addBallJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position);
	}

	for (unsigned int i = 0; i < data.m_ballOnLineJointData.size(); i++)
	{
		const SceneLoader::BallOnLineJointData &jd = data.m_ballOnLineJointData[i];
		model->addBallOnLineJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position, jd.m_axis);
	}

	for (unsigned int i = 0; i < data.m_hingeJointData.size(); i++)
	{
		const SceneLoader::HingeJointData &jd = data.m_hingeJointData[i];
		model->addHingeJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position, jd.m_axis);
	}

	for (unsigned int i = 0; i < data.m_universalJointData.size(); i++)
	{
		const SceneLoader::UniversalJointData &jd = data.m_universalJointData[i];
		model->addUniversalJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position, jd.m_axis[0], jd.m_axis[1]);
	}

	for (unsigned int i = 0; i < data.m_sliderJointData.size(); i++)
	{
		const SceneLoader::SliderJointData &jd = data.m_sliderJointData[i];
		model->addSliderJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_axis);
	}

	for (unsigned int i = 0; i < data.m_rigidBodyParticleBallJointData.size(); i++)
	{
		const SceneLoader::RigidBodyParticleBallJointData &jd = data.m_rigidBodyParticleBallJointData[i];
		model->addRigidBodyParticleBallJoint(id_index[jd.m_bodyID[0]], jd.m_bodyID[1]);
	}

	for (unsigned int i = 0; i < data.m_targetAngleMotorHingeJointData.size(); i++)
	{
		const SceneLoader::TargetAngleMotorHingeJointData &jd = data.m_targetAngleMotorHingeJointData[i];
		model->addTargetAngleMotorHingeJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position, jd.m_axis);
		((MotorJoint*)constraints[constraints.size() - 1])->setTarget(jd.m_target);
		((MotorJoint*)constraints[constraints.size() - 1])->setTargetSequence(jd.m_targetSequence);
		((MotorJoint*)constraints[constraints.size() - 1])->setRepeatSequence(jd.m_repeat);
	}

	for (unsigned int i = 0; i < data.m_targetVelocityMotorHingeJointData.size(); i++)
	{
		const SceneLoader::TargetVelocityMotorHingeJointData &jd = data.m_targetVelocityMotorHingeJointData[i];
		model->addTargetVelocityMotorHingeJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position, jd.m_axis);
		((MotorJoint*)constraints[constraints.size() - 1])->setTarget(jd.m_target);
		((MotorJoint*)constraints[constraints.size() - 1])->setTargetSequence(jd.m_targetSequence);
		((MotorJoint*)constraints[constraints.size() - 1])->setRepeatSequence(jd.m_repeat);
	}

	for (unsigned int i = 0; i < data.m_targetPositionMotorSliderJointData.size(); i++)
	{
		const SceneLoader::TargetPositionMotorSliderJointData &jd = data.m_targetPositionMotorSliderJointData[i];
		model->addTargetPositionMotorSliderJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_axis);
		((MotorJoint*)constraints[constraints.size() - 1])->setTarget(jd.m_target);
		((MotorJoint*)constraints[constraints.size() - 1])->setTargetSequence(jd.m_targetSequence);
		((MotorJoint*)constraints[constraints.size() - 1])->setRepeatSequence(jd.m_repeat);
	}

	for (unsigned int i = 0; i < data.m_targetVelocityMotorSliderJointData.size(); i++)
	{
		const SceneLoader::TargetVelocityMotorSliderJointData &jd = data.m_targetVelocityMotorSliderJointData[i];
		model->addTargetVelocityMotorSliderJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_axis);
		((MotorJoint*)constraints[constraints.size() - 1])->setTarget(jd.m_target);
		((MotorJoint*)constraints[constraints.size() - 1])->setTargetSequence(jd.m_targetSequence);
		((MotorJoint*)constraints[constraints.size() - 1])->setRepeatSequence(jd.m_repeat);
	}

	for (unsigned int i = 0; i < data.m_damperJointData.size(); i++)
	{
		const SceneLoader::DamperJointData &jd = data.m_damperJointData[i];
		model->addDamperJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_axis, jd.m_stiffness);
	}

	for (unsigned int i = 0; i < data.m_rigidBodySpringData.size(); i++)
	{
		const SceneLoader::RigidBodySpringData &jd = data.m_rigidBodySpringData[i];
		model->addRigidBodySpring(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position1, jd.m_position2, jd.m_stiffness);
	}

	for (unsigned int i = 0; i < data.m_distanceJointData.size(); i++)
	{
		const SceneLoader::DistanceJointData &jd = data.m_distanceJointData[i];
		model->addDistanceJoint(id_index[jd.m_bodyID[0]], id_index[jd.m_bodyID[1]], jd.m_position1, jd.m_position2);
	}
}

void TW_CALL setContactTolerance(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((DistanceFieldCollisionDetection*)clientData)->setTolerance(val);
}

void TW_CALL getContactTolerance(void *value, void *clientData)
{
	*(Real *)(value) = ((DistanceFieldCollisionDetection*)clientData)->getTolerance();
}

void TW_CALL setContactStiffnessRigidBody(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setContactStiffnessRigidBody(val);
}

void TW_CALL getContactStiffnessRigidBody(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getContactStiffnessRigidBody();
}

void TW_CALL setContactStiffnessParticleRigidBody(const void *value, void *clientData)
{
	const Real val = *(const Real *)(value);
	((SimulationModel*)clientData)->setContactStiffnessParticleRigidBody(val);
}

void TW_CALL getContactStiffnessParticleRigidBody(void *value, void *clientData)
{
	*(Real *)(value) = ((SimulationModel*)clientData)->getContactStiffnessParticleRigidBody();
}

void TW_CALL setBendingMethod(const void *value, void *clientData)
{
	const short val = *(const short *)(value);
	*((short*)clientData) = val;
	reset();
}

void TW_CALL getBendingMethod(void *value, void *clientData)
{
	*(short *)(value) = *((short*)clientData);
}

void TW_CALL setClothSimulationMethod(const void *value, void *clientData)
{
	const short val = *(const short *)(value);
	*((short*)clientData) = val;
	reset();
}

void TW_CALL getClothSimulationMethod(void *value, void *clientData)
{
	*(short *)(value) = *((short*)clientData);
}

void TW_CALL setSolidSimulationMethod(const void *value, void *clientData)
{
	const short val = *(const short *)(value);
	*((short*)clientData) = val;
	reset();
}

void TW_CALL getSolidSimulationMethod(void *value, void *clientData)
{
	*(short *)(value) = *((short*)clientData);
}

void TW_CALL setBendingStiffness(const void* value, void* clientData)
{
	bendingStiffness = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<DihedralConstraint, Real, &DihedralConstraint::m_stiffness>(bendingStiffness);
	((SimulationModel*)clientData)->setConstraintValue<IsometricBendingConstraint, Real, &IsometricBendingConstraint::m_stiffness>(bendingStiffness);
	((SimulationModel*)clientData)->setConstraintValue<IsometricBendingConstraint_XPBD, Real, &IsometricBendingConstraint_XPBD::m_stiffness>(bendingStiffness);
}

void TW_CALL getBendingStiffness(void* value, void* clientData)
{
	*(Real*)(value) = bendingStiffness;
}

void TW_CALL setDistanceStiffness(const void* value, void* clientData)
{
	distanceStiffness = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<DistanceConstraint, Real, &DistanceConstraint::m_stiffness>(distanceStiffness);
	((SimulationModel*)clientData)->setConstraintValue<DistanceConstraint_XPBD, Real, &DistanceConstraint_XPBD::m_stiffness>(distanceStiffness);
}

void TW_CALL getDistanceStiffness(void* value, void* clientData)
{
	*(Real*)(value) = distanceStiffness;
}

void TW_CALL setXXStiffness(const void* value, void* clientData)
{
	xxStiffness = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<FEMTriangleConstraint, Real, &FEMTriangleConstraint::m_xxStiffness>(xxStiffness);
	((SimulationModel*)clientData)->setConstraintValue<StrainTriangleConstraint, Real, &StrainTriangleConstraint::m_xxStiffness>(xxStiffness);
}

void TW_CALL getXXStiffness(void* value, void* clientData)
{
	*(Real*)(value) = xxStiffness;
}

void TW_CALL getYYStiffness(void* value, void* clientData)
{
	*(Real*)(value) = yyStiffness;
}

void TW_CALL setYYStiffness(const void* value, void* clientData)
{
	yyStiffness = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<FEMTriangleConstraint, Real, &FEMTriangleConstraint::m_yyStiffness>(yyStiffness);
	((SimulationModel*)clientData)->setConstraintValue<StrainTriangleConstraint, Real, &StrainTriangleConstraint::m_yyStiffness>(yyStiffness);
}

void TW_CALL getXYStiffness(void* value, void* clientData)
{
	*(Real*)(value) = xyStiffness;
}

void TW_CALL setXYStiffness(const void* value, void* clientData)
{
	xyStiffness = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<FEMTriangleConstraint, Real, &FEMTriangleConstraint::m_xyStiffness>(xyStiffness);
	((SimulationModel*)clientData)->setConstraintValue<StrainTriangleConstraint, Real, &StrainTriangleConstraint::m_xyStiffness>(xyStiffness);
}

void TW_CALL getXYPoissonRatio(void* value, void* clientData)
{
	*(Real*)(value) = xyPoissonRatio;
}

void TW_CALL setXYPoissonRatio(const void* value, void* clientData)
{
	xyPoissonRatio = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<FEMTriangleConstraint, Real, &FEMTriangleConstraint::m_xyPoissonRatio>(xyPoissonRatio);
}

void TW_CALL getYXPoissonRatio(void* value, void* clientData)
{
	*(Real*)(value) = yxPoissonRatio;
}

void TW_CALL setYXPoissonRatio(const void* value, void* clientData)
{
	yxPoissonRatio = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<FEMTriangleConstraint, Real, &FEMTriangleConstraint::m_yxPoissonRatio>(yxPoissonRatio);
}

void TW_CALL getNormalizeStretch(void* value, void* clientData)
{
	*(bool*)(value) = normalizeStretch;
}

void TW_CALL setNormalizeStretch(const void* value, void* clientData)
{
	normalizeStretch = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<StrainTriangleConstraint, bool, &StrainTriangleConstraint::m_normalizeStretch>(normalizeStretch);
}

void TW_CALL getNormalizeShear(void* value, void* clientData)
{
	*(bool*)(value) = normalizeShear;
}

void TW_CALL setNormalizeShear(const void* value, void* clientData)
{
	normalizeShear = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<StrainTriangleConstraint, bool, &StrainTriangleConstraint::m_normalizeShear>(normalizeShear);
}

void TW_CALL setSolidStiffness(const void* value, void* clientData)
{
	solidStiffness = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<FEMTetConstraint, Real, &FEMTetConstraint::m_stiffness>(solidStiffness);
	((SimulationModel*)clientData)->setConstraintValue<XPBD_FEMTetConstraint, Real, &XPBD_FEMTetConstraint::m_stiffness>(solidStiffness);
	((SimulationModel*)clientData)->setConstraintValue<StrainTetConstraint, Real, &StrainTetConstraint::m_stretchStiffness>(solidStiffness);
	((SimulationModel*)clientData)->setConstraintValue<StrainTetConstraint, Real, &StrainTetConstraint::m_shearStiffness>(solidStiffness);
	((SimulationModel*)clientData)->setConstraintValue<DistanceConstraint, Real, &DistanceConstraint::m_stiffness>(solidStiffness);
	((SimulationModel*)clientData)->setConstraintValue<DistanceConstraint_XPBD, Real, &DistanceConstraint_XPBD::m_stiffness>(solidStiffness);
	((SimulationModel*)clientData)->setConstraintValue<ShapeMatchingConstraint, Real, &ShapeMatchingConstraint::m_stiffness>(solidStiffness);
}

void TW_CALL getSolidStiffness(void* value, void* clientData)
{
	*(Real*)(value) = solidStiffness;
}

void TW_CALL setVolumeStiffness(const void* value, void* clientData)
{
	volumeStiffness = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<VolumeConstraint, Real, &VolumeConstraint::m_stiffness>(volumeStiffness);
	((SimulationModel*)clientData)->setConstraintValue<VolumeConstraint_XPBD, Real, &VolumeConstraint_XPBD::m_stiffness>(volumeStiffness);
}

void TW_CALL getVolumeStiffness(void* value, void* clientData)
{
	*(Real*)(value) = volumeStiffness;
}

void TW_CALL getSolidPoissonRatio(void* value, void* clientData)
{
	*(Real*)(value) = solidPoissonRatio;
}

void TW_CALL setSolidPoissonRatio(const void* value, void* clientData)
{
	solidPoissonRatio = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<FEMTetConstraint, Real, &FEMTetConstraint::m_poissonRatio>(solidPoissonRatio);
	((SimulationModel*)clientData)->setConstraintValue<XPBD_FEMTetConstraint, Real, &XPBD_FEMTetConstraint::m_poissonRatio>(solidPoissonRatio);
}

void TW_CALL getSolidNormalizeStretch(void* value, void* clientData)
{
	*(bool*)(value) = solidNormalizeStretch;
}

void TW_CALL setSolidNormalizeStretch(const void* value, void* clientData)
{
	solidNormalizeStretch = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<StrainTetConstraint, bool, &StrainTetConstraint::m_normalizeStretch>(solidNormalizeStretch);
}

void TW_CALL getSolidNormalizeShear(void* value, void* clientData)
{
	*(bool*)(value) = solidNormalizeShear;
}

void TW_CALL setSolidNormalizeShear(const void* value, void* clientData)
{
	solidNormalizeShear = *(const Real*)(value);
	((SimulationModel*)clientData)->setConstraintValue<StrainTetConstraint, bool, &StrainTetConstraint::m_normalizeShear>(solidNormalizeShear);
}