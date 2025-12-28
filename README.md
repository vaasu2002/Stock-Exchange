# Prototyping-IPC


#Installation
```bash
chmod +x scripts/install-deps.sh
./scripts/install-deps.sh 
```

mkdir build && cd build

cmake ..

cmake --build .

./process1 9001 & ./process2 9002 &