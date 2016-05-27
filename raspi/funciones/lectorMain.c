/****************************************************************************
 * lectorMain() lanza los procesos para lectura y escritura del dispositivo
 * (truculenti a tope)
 *****************/
int lectorMain() {
  char * funName = "lectorMain";
  
    /* parte común con los pipes */
    int to_cu_pipe[2];
    int from_cu_pipe[2];

    /** mandato del SO de tipo "connect unix" que es lo más sencillito para
      *   interactuar con el canal serie del Arduino a alta velocidad.
      *   ¡ojo con la velocidad del USB en el lado del Arduino!
      **/
    char *cmd[] = { "/usr/bin/cu", "-l", "/dev/ttyACM0", "-s", "230400", 0 };
    /* char *cmd[] = { "/bin/cat", 0 }; * perhaps some more easy to debug */

    /* Create the pipes.   */
    if (pipe (to_cu_pipe)) {
         fprintf (stderr, "Pipe to_cu_pipe failed.\n");
       exit_status = EXIT_FAIL_CANAL; /* fallo en dup2 o pipe */
         comSHM(SERRCANAL);
         return exit_status;
    }
    if (pipe (from_cu_pipe)) {
         fprintf (stderr, "Pipe from_cu_pipe failed.\n");
         exit_status = EXIT_FAIL_CANAL; /* fallo en dup2 o pipe */
         comSHM(SERRCANAL);
         return exit_status;
    }

    extern int exit_status;

    /* fprintf(stderr, "voy a hacer fork\n"); fflush(stderr); */
    /* Create the child process.   */
    switch (fork ()) {
    case 0:            /* CHILD: this is our CU/device !! */
       dup2(to_cu_pipe[0], 0) ;   /* stdin to reading from the parent, and... */
       dup2(from_cu_pipe[1], 1) ; /* stdout to write to the parent. */
       close(to_cu_pipe[1]);
       close(from_cu_pipe[0]);
       /* fprintf(stderr, "justo antes de execvp\n"); fflush(stderr); */
       /* ejecutar el CU */
       execvp(cmd[0], cmd);
       perror(cmd[0]); /* execvp failed */
                               /* las razones son peculiares de cu */
       exit_status = EXIT_FAIL_EXEC; /* fallo en el proceso de lectura cu */
       comSHM(SERREXEC);
       break;
    case -1: /* The fork failed.   */
       /* fprintf (stderr, "Fork failed.\nPor alguna razón\n"); */
       exit_status = EXIT_FAIL_FORK;
       comSHM(SERRFORK);
       break;
    default:          /* This is the parent process.   */
       {
          FILE *stream_out; /* stream to send conf-parameters to CU/device */
          stream_out = fdopen (to_cu_pipe[1], "w");
   
   
          FILE *stream_in; /* stream to read data from CU/device */
          stream_in = fdopen (from_cu_pipe[0], "r");
   
          close(to_cu_pipe[0]);
          close(from_cu_pipe[1]);
          /* fprintf(stderr, "Hacia CU desde el padre\n"); fflush(stderr); */
          /* give CU/device some time to wake-up? */
          /* (void) sleep(1); */
          DEP_1("Antes de dataDump");
          dataDump(stream_in, stream_out, nombreArchivo);   /*** DATA_DUMP() ***/
          
          DEP_1("Despues de dataDump");
          comSHM(SREADY); /* listo para la siguiente adquisición */
          /* matamos al proceso hijo elegantemente cerrando un pipe */
          fclose (stream_out);
   
          int proc_status;
   
          while (wait(&proc_status) != -1) ; /* pick up dead children */
             
          exit_status = EXIT_STATUS_OK; /* sin fallo */
          comSHM(SREADY); /* nuestra forma de indicar que no ha habido fallo */
          break;
       }
    }
    return exit_status;
}
