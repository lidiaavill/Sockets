# lanzaServidor.sh
# Lanza el servidor que es un daemon y varios clientes
# las ordenes estAn en un fichero que se pasa como tercer parAmetro
./servidor
./clientcp localhost TCP ordenes.txt &
./clientcp localhost TCP ordenes1.txt &
./clientcp localhost TCP ordenes2.txt &

