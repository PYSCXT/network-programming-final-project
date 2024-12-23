include ../Make.defines

PROGS = server client

all: ${PROGS}

server: server.o
	${CC} ${CFLAGS} -o $@ server.o ${LIBS}

client: client.o
	${CC} ${CFLAGS} -o $@ client.o ${LIBS}

clean:
	rm -f ${PROGS} ${CLEANFILES}