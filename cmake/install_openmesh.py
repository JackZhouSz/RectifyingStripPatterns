import os
current_file= (os.path.abspath(__file__))
dirname = os.path.dirname(current_file)


ompath=dirname+"/../external/openmesh/OpenMesh-9.1.0"
os.system("mkdir "+ompath+"/build")
os.system("cmake -B"+ompath+"/build -H"+ompath)
os.system("make -j4 -C "+ompath+"/build")