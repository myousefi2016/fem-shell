#include <iostream>
#include <stdlib.h>
#include <math.h>
#include "precice/SolverInterface.hpp"

using std::cout;
using std::endl;

using namespace precice;
using namespace precice::constants;

void printData (const std::vector<double>& data)
{
  std::cout << "Received data = " << data[0];
  for (size_t i=1; i < data.size(); i++){
    std::cout << ", " << data[i];
  }
  std::cout << std::endl;
}

int main (int argc, char **argv)
{
  cout << "Starting Fluid Solver Dummy..." << endl;

  if ( argc != 4 )
  {
    cout << endl;
    cout << "Usage: " << argv[0] <<  " configurationFileName N tau" << endl;
    cout << endl;
    cout << "N:     Number of mesh elements, needs to be equal for fluid and structure solver." << endl;
    cout << "tau:   Dimensionless time step size." << endl;
    return -1;
  }

  std::string configFileName(argv[1]);
  int N        = atoi( argv[2] );
  double tau   = atof( argv[3] );

  std::cout << "N: " << N << " tau: " << tau << std::endl;

  std::string solverName = "FLUID";

  cout << "Configure preCICE..." << endl;
  // Initialize the solver interface with our name, our process index (like rank) and the total number of processes.
  SolverInterface interface(solverName, 0, 1);
  // Provide the configuration file to precice. After configuration a usuable state of that SolverInterface is reached.
  // Reads the XML file and contacts the server, if used.
  interface.configure(configFileName);
  cout << "preCICE configured..." << endl;

  // init data
  int i;
  double *f, *f_n, *d, *d_n;
  int dimensions = interface.getDimensions();

  f     = new double[N*dimensions]; // Force
  f_n   = new double[N*dimensions];
  d     = new double[N*dimensions]; // Displacements
  d_n   = new double[N*dimensions];

  //precice stuff
  int meshID = interface.getMeshID("Fluid_Nodes");
  int dID = interface.getDataID("Displacements", meshID);
  int fID = interface.getDataID("Forces", meshID);
  int *vertexIDs;
  vertexIDs = new int[N];
  double *grid;
  grid = new double[dimensions*N];

  int sqrtN = sqrt(N);
  const float meshSize = 10.0;
  float tileSize = meshSize/(float)sqrtN;
  cout << "sqrt(" << N << ") = " << sqrtN << endl;
  for (i = 0; i < N; i++)
  {
    for (int dim = 0; dim < dimensions; dim++)
    {
      d[i*dimensions+dim]   = 1.0;
      d_n[i*dimensions+dim] = 1.0;
      f[i*dimensions+dim]   = 0.0;
      f_n[i*dimensions+dim] = 0.0;
    }
    
    grid[i*dimensions]   = (i%sqrtN) * tileSize;
    grid[i*dimensions+1] = (i/sqrtN) * tileSize;
    grid[i*dimensions+2] = -0.01;
  }

  int t = 0; //number of timesteps

  interface.setMeshVertices(meshID, N, grid, vertexIDs);

  cout << "Fluid: init precice..." << endl;
  interface.initialize();


  if (interface.isActionRequired(actionWriteInitialData()))
  {
    interface.writeBlockVectorData(fID, N, vertexIDs, f);
    //interface.initializeData();
    interface.fulfilledAction(actionWriteInitialData());
  }

  interface.initializeData();

  if (interface.isReadDataAvailable())
  {
    interface.readBlockVectorData(dID, N, vertexIDs, d);
  }

  while (interface.isCouplingOngoing())
  {
    // When an implicit coupling scheme is used, checkpointing is required
    if (interface.isActionRequired(actionWriteIterationCheckpoint()))
    {
      interface.fulfilledAction(actionWriteIterationCheckpoint());
    }

    f[12*3+2] = 1.0;

    interface.writeBlockVectorData(fID, N, vertexIDs, f);
    interface.advance(1.0);
    interface.readBlockVectorData(dID, N, vertexIDs, d);

    if (interface.isActionRequired(actionReadIterationCheckpoint()))
    { // i.e. not yet converged
      cout << "Iterate" << endl;
      interface.fulfilledAction(actionReadIterationCheckpoint());
    }
    else
    {
      cout << "Fluid: Advancing in time, finished timestep: " << t << endl;
      t++;

      for ( i = 0; i < N; i++)
      {
	for (int dim = 0; dim < dimensions; dim++)
        {
          f_n[i*dimensions+dim] = f[i*dimensions+dim];
          d_n[i*dimensions+dim] = d[i*dimensions+dim];
        }
      }
    }
  }

  interface.finalize();
  cout << "Exiting FluidSolver" << endl;

  return 0;
}