ALL: V2V

V2V: V2V.c
	mpicc V2V.c -lpthread -o simulation 

run:
	mpirun -oversubscribe -np 8 simulation

clean :
	/bin/rm -f simulation *.o