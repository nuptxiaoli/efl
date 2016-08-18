#define EFL_BETA_API_SUPPORT 1
#define EFL_EO_API_SUPPORT 1
#include <Ecore.h>
#include <Ecore_Con.h>
#include <Ecore_Getopt.h>
#include <fcntl.h>

static int retval = EXIT_SUCCESS;

/* NOTE: input events are only used as debug, you can omit these */

static void
_input_can_read_changed(void *data EINA_UNUSED, const Eo_Event *event)
{
   fprintf(stderr, "INFO: input can_read=%d\n",
           efl_io_reader_can_read_get(event->object));
}

static void
_input_eos(void *data EINA_UNUSED, const Eo_Event *event)
{
   fprintf(stderr, "INFO: input eos=%d\n",
           efl_io_reader_eos_get(event->object));
}

EFL_CALLBACKS_ARRAY_DEFINE(input_cbs,
                           { EFL_IO_READER_EVENT_CAN_READ_CHANGED, _input_can_read_changed },
                           { EFL_IO_READER_EVENT_EOS, _input_eos });


/* NOTE: output events are only used as debug, you can omit these */

static void
_output_can_write_changed(void *data EINA_UNUSED, const Eo_Event *event)
{
   fprintf(stderr, "INFO: output can_write=%d\n",
           efl_io_writer_can_write_get(event->object));
}

EFL_CALLBACKS_ARRAY_DEFINE(output_cbs,
                           { EFL_IO_WRITER_EVENT_CAN_WRITE_CHANGED, _output_can_write_changed });

static void
_output_buffer_reallocated(void *data EINA_UNUSED, const Eo_Event *event)
{
   Eina_Slice slice;

   if (!efl_io_buffer_slice_get(event->object, &slice))
     {
        fprintf(stderr, "ERROR: could not get buffer slice\n");
        return;
     }

   fprintf(stderr, "INFO: output buffer reallocated=" EINA_SLICE_FMT "\n",
           EINA_SLICE_PRINT(slice));
}

EFL_CALLBACKS_ARRAY_DEFINE(output_buffer_cbs,
                           { EFL_IO_BUFFER_EVENT_REALLOCATED, _output_buffer_reallocated });


static void
_dialer_resolved(void *data EINA_UNUSED, const Eo_Event *event)
{
   fprintf(stderr, "INFO: dialer resolved '%s' to '%s'\n",
           efl_net_dialer_address_dial_get(event->object),
           efl_net_socket_address_remote_get(event->object));
}

static void
_dialer_error(void *data EINA_UNUSED, const Eo_Event *event)
{
   const Eina_Error *perr = event->info;
   fprintf(stderr, "INFO: error: %d\n", *perr);
   retval = EXIT_FAILURE;
   /* no need to quit as copier will get a "eos" event and emit "done" */
}

static void
_dialer_connected(void *data EINA_UNUSED, const Eo_Event *event)
{
   fprintf(stderr, "INFO: dialer connected to '%s' (%s)\n",
           efl_net_dialer_address_dial_get(event->object),
           efl_net_socket_address_remote_get(event->object));
}

EFL_CALLBACKS_ARRAY_DEFINE(dialer_cbs,
                           { EFL_NET_DIALER_EVENT_RESOLVED, _dialer_resolved },
                           { EFL_NET_DIALER_EVENT_ERROR, _dialer_error },
                           { EFL_NET_DIALER_EVENT_CONNECTED, _dialer_connected });

/* copier events are of interest, you should hook to at least "done"
 * and "error"
 */

static void
_copier_done(void *data EINA_UNUSED, const Eo_Event *event)
{
   Eo *destination = efl_io_copier_destination_get(event->object);

   if (!destination)
     {
        /* if :none: was used, you get Efl_Io_Copier to buffer all
         * source data up to efl_io_copier_buffer_limit_get().
         *
         * Then if it finished, you can steal the binbuf and do
         * something with that. It's a simple way to use
         * Efl_Io_Copier.
         */
        Eina_Binbuf *binbuf = efl_io_copier_binbuf_steal(event->object);

        fprintf(stderr,
                "INFO: :none: resulted in binbuf=%p, string=%p, size=%zd:"
                "\n--BEGIN DATA--\n"
                EINA_SLICE_STR_FMT
                "\n--END DATA--\n",
                binbuf,
                eina_binbuf_string_get(binbuf),
                eina_binbuf_length_get(binbuf),
                EINA_SLICE_STR_PRINT(eina_binbuf_slice_get(binbuf))
                );

        eina_binbuf_free(binbuf);
     }
   else if (efl_isa(destination, EFL_IO_BUFFER_CLASS))
     {
        /* if :memory: was used, then an Efl_Io_Buffer is created
         * implementing all the interfaces required by Efl_Io_Copier.
         *
         * This allows the buffer to be resized if source size is
         * known, avoiding realloc()s during execution.
         *
         * You can use:
         *
         *  - efl_io_buffer_slice_get() for a read-only view of that
         *    buffer,
         *
         * - efl_io_buffer_binbuf_steal() to take the buffer memory as
         *   an Eina_Binbuf, you own the ownership and must call
         *   eina_binbuf_free() to release it.
         */
        Eina_Slice slice;

        if (!efl_io_buffer_slice_get(destination, &slice))
          fprintf(stderr, "ERROR: could not get buffer slice\n");
        else
          fprintf(stderr,
                  "INFO: :memory: resulted in slice=" EINA_SLICE_FMT ":"
                  "\n--BEGIN DATA--\n"
                  EINA_SLICE_STR_FMT
                  "\n--END DATA--\n",
                  EINA_SLICE_PRINT(slice), EINA_SLICE_STR_PRINT(slice));
     }

   fprintf(stderr, "INFO: done\n");
   ecore_main_loop_quit();
}

static void
_copier_error(void *data EINA_UNUSED, const Eo_Event *event)
{
   const Eina_Error *perr = event->info;
   fprintf(stderr, "INFO: error: %d\n", *perr);
   retval = EXIT_FAILURE;
   ecore_main_loop_quit();
}

static void
_copier_progress(void *data EINA_UNUSED, const Eo_Event *event)
{
   uint64_t r, w, t;

   efl_io_copier_progress_get(event->object, &r, &w, &t);
   /* if total is zero, that means the source object doesn't provide a
    * fixed size, consider it a "stream" such as a socket, a pipe,
    * stdin...
    */
   if (t == 0)
     fprintf(stderr, "INFO: read=%" PRIu64 ", written=%" PRIu64 "\n", r, w);
   else
     {
        fprintf(stderr,
                "INFO: read=%" PRIu64 ", written=%" PRIu64 ", total=%" PRIu64
                " (%4.1f%%/%4.1f%%)\n",
                r, w, t,
                (100.0 * r) / (double)t,
                (100.0 * w) / (double)t);
     }
}

static void
_copier_data(void *data EINA_UNUSED, const Eo_Event *event)
{
   const Eina_Slice *slice = event->info;
   /* a piece of data was processed, it's ready-only and will only be
    * good for immediate consumption.
    *
    * It's only usable inside this function as it may be gone (freed,
    * reallocated) once it return and more data is processed.
    *
    * Shall you want to take over the internal binbuf, use
    * efl_io_copier_binbuf_steal() and then stop event propagation
    * with efl_event_callback_stop().
    *
    * However be aware that other events will be dispatched with empty
    * slices, like if you steal the buffer here, _copier_line() will
    * get empty slice.
    */
   fprintf(stderr, "INFO: data: " EINA_SLICE_FMT "\n",
           EINA_SLICE_PRINT(*slice));
}

static void
_copier_line(void *data EINA_UNUSED, const Eo_Event *event)
{
   const Eina_Slice *slice = event->info;

   /* a line_delimiter was provided and a line was processed, it's
    * ready-only and will only be good for immediate consumption.
    *
    * It's only usable inside this function as it may be gone (freed,
    * reallocated) once it return and more data is processed.
    *
    * The line may not contain the line delimiter in the following
    * cases:
    *
    * - efl_io_copier_buffer_limit_set() was used and limit was
    *   reached.
    *
    * - source reached end-of-stream and pending data was there.
    *
    * Shall you want to take over the internal binbuf, use
    * efl_io_copier_binbuf_steal() and then stop event propagation
    * with efl_event_callback_stop().
    */

   fprintf(stderr, "INFO: line: " EINA_SLICE_STR_FMT "\n",
           EINA_SLICE_STR_PRINT(*slice));
}

EFL_CALLBACKS_ARRAY_DEFINE(copier_cbs,
                           { EFL_IO_COPIER_EVENT_DONE, _copier_done },
                           { EFL_IO_COPIER_EVENT_ERROR, _copier_error },
                           { EFL_IO_COPIER_EVENT_PROGRESS, _copier_progress },
                           { EFL_IO_COPIER_EVENT_DATA, _copier_data},
                           { EFL_IO_COPIER_EVENT_LINE, _copier_line});


static const Ecore_Getopt options = {
  "efl_io_copier_example", /* program name */
  NULL, /* usage line */
  "1", /* version */
  "(C) 2016 Enlightenment Project", /* copyright */
  "BSD 2-Clause", /* license */
  /* long description, may be multiline and contain \n */
  "Example of Efl_Io_Copier usage.\n"
  "\n"
  "This example copies from an Efl_Io_Reader to an Efl_Io_Writer, listening"
  "for events and showing progress.",
  EINA_FALSE,
  {
    ECORE_GETOPT_STORE_STR('d', "line-delimiter",
                           "If set will define a line delimiter for copy operation, instead of a fixed chunk size. This will trigger line events."),
    ECORE_GETOPT_STORE_ULONG('l', "buffer-limit",
                             "If set will limit buffer size to this limit of bytes. If used alongside with --line-delimiter and that delimiter was not found but bffer limit was reached, the line event will be triggered without the delimiter at the end."),
    ECORE_GETOPT_STORE_ULONG('c', "read-chunk-size",
                             "If set will change the base chunk size used while reading."),
    ECORE_GETOPT_VERSION('V', "version"),
    ECORE_GETOPT_COPYRIGHT('C', "copyright"),
    ECORE_GETOPT_LICENSE('L', "license"),
    ECORE_GETOPT_HELP('h', "help"),

    ECORE_GETOPT_STORE_METAVAR_STR(0, NULL,
                                   "The input file name or:\n"
                                   ":stdin: to read from stdin.\n"
                                   "tcp://IP:PORT to connect using TCP and an IPv4 (A.B.C.D:PORT) or IPv6 ([A:B:C:D::E]:PORT).\n"
                                   "",
                                   "input-file"),
    ECORE_GETOPT_STORE_METAVAR_STR(0, NULL,
                                   "The output file name or:\n"
                                   ":stdout: to write to stdout.\n"
                                   ":stderr: to write to stderr.\n"
                                   ":memory: to write to a memory buffer.\n"
                                   ":none: to not use a destination object.\n"
                                   "tcp://IP:PORT to connect using TCP and an IPv4 (A.B.C.D:PORT) or IPv6 ([A:B:C:D::E]:PORT).\n"
                                   "",
                                   "output-file"),
    ECORE_GETOPT_SENTINEL
  }
};

int
main(int argc, char **argv)
{
   char *input_fname = NULL;
   char *output_fname = NULL;
   char *line_delimiter = NULL;
   unsigned long buffer_limit = 0;
   unsigned long read_chunk_size = 0;
   Eina_Bool quit_option = EINA_FALSE;
   Ecore_Getopt_Value values[] = {
     ECORE_GETOPT_VALUE_STR(line_delimiter),
     ECORE_GETOPT_VALUE_ULONG(buffer_limit),
     ECORE_GETOPT_VALUE_ULONG(read_chunk_size),

     /* standard block to provide version, copyright, license and help */
     ECORE_GETOPT_VALUE_BOOL(quit_option), /* -V/--version quits */
     ECORE_GETOPT_VALUE_BOOL(quit_option), /* -C/--copyright quits */
     ECORE_GETOPT_VALUE_BOOL(quit_option), /* -L/--license quits */
     ECORE_GETOPT_VALUE_BOOL(quit_option), /* -h/--help quits */

     /* positional argument */
     ECORE_GETOPT_VALUE_STR(input_fname),
     ECORE_GETOPT_VALUE_STR(output_fname),

     ECORE_GETOPT_VALUE_NONE /* sentinel */
   };
   int args;
   Eo *input, *output, *copier;
   Eina_Slice line_delm_slice = EINA_SLICE_STR_LITERAL("");

   ecore_init();
   ecore_con_init();

   args = ecore_getopt_parse(&options, values, argc, argv);
   if (args < 0)
     {
        fputs("ERROR: Could not parse command line options.\n", stderr);
        retval = EXIT_FAILURE;
        goto end;
     }

   if (quit_option) goto end;

   args = ecore_getopt_parse_positional(&options, values, argc, argv, args);
   if (args < 0)
     {
        fputs("ERROR: Could not parse positional arguments.\n", stderr);
        retval = EXIT_FAILURE;
        goto end;
     }

   /* Efl_Io_Copier works with any object that implements
    * Efl_Io_Reader and Efl_Io_Writer interfaces. Here we create
    * couple of different objects to showcase that.
    *
    * Note that input_cbs(), output_cbs() are OPTIONAL, here are only
    * used to print out how each object behaves.
    */

   if (strcmp(input_fname, ":stdin:") == 0)
     {
        input = efl_add(EFL_IO_STDIN_CLASS, NULL,
                        efl_event_callback_array_add(efl_self, input_cbs(), NULL));
        if (!input)
          {
             fprintf(stderr, "ERROR: could not open stdin.\n");
             retval = EXIT_FAILURE;
             goto end;
          }
     }
   else if (strncmp(input_fname, "tcp://", strlen("tcp://")) == 0)
     {
        /*
         * Since Efl.Net.Socket implements the required interfaces,
         * they can be used here as well.
         */
        const char *address = input_fname + strlen("tcp://");
        Eina_Error err;
        input = efl_add(EFL_NET_DIALER_TCP_CLASS, NULL,
                        efl_event_callback_array_add(efl_self, input_cbs(), NULL), /* optional */
                        efl_event_callback_array_add(efl_self, dialer_cbs(), NULL) /* optional */
                        );
        if (!input)
          {
             fprintf(stderr, "ERROR: could not create TCP Dialer.\n");
             retval = EXIT_FAILURE;
             goto end;
          }

        err = efl_net_dialer_dial(input, address);
        if (err)
          {
             fprintf(stderr, "ERROR: could not TCP dial %s: %s\n",
                     address, eina_error_msg_get(err));
             goto end_input;
          }
     }
   else
     {
        /* regular file, open with flags: read-only and close-on-exec */
        input = efl_add(EFL_IO_FILE_CLASS, NULL,
                        efl_file_set(efl_self, input_fname, NULL), /* mandatory */
                        efl_io_file_flags_set(efl_self, O_RDONLY | O_CLOEXEC), /* recommended */
                        efl_event_callback_array_add(efl_self, input_cbs(), NULL) /* optional */
                        );
        if (!input)
          {
             fprintf(stderr, "ERROR: could not open '%s' for read.\n",
                     input_fname);
             retval = EXIT_FAILURE;
             goto end;
          }
     }

   if (strcmp(output_fname, ":stdout:") == 0)
     {
        output = efl_add(EFL_IO_STDOUT_CLASS, NULL,
                         efl_event_callback_array_add(efl_self, output_cbs(), NULL) /* optional */
                         );
        if (!output)
          {
             fprintf(stderr, "ERROR: could not open stdout.\n");
             retval = EXIT_FAILURE;
             goto end_input;
          }
     }
   else if (strcmp(output_fname, ":stderr:") == 0)
     {
        output = efl_add(EFL_IO_STDERR_CLASS, NULL,
                         efl_event_callback_array_add(efl_self, output_cbs(), NULL) /* optional */
                         );
        if (!output)
          {
             fprintf(stderr, "ERROR: could not open stderr.\n");
             retval = EXIT_FAILURE;
             goto end_input;
          }
     }
   else if (strcmp(output_fname, ":memory:") == 0)
     {
        /*
         * This uses a memory destination, then the process will
         * result in that object holding all the memory.
         *
         * One can use things like efl_io_buffer_limit_set() to limit
         * it's size.
         *
         * If the source object provides a size (ie: a file), then
         * this buffer will be resized only once at the start, saving
         * reallocs. Otherwise it will grow as needed.  Contrast this
         * with ":none:" method below, that always resize.
         *
         * When finished get the efl_io_buffer_slice_get(), see
         * _copier_done().
         */
        output = efl_add(EFL_IO_BUFFER_CLASS, NULL,
                         efl_event_callback_array_add(efl_self, output_cbs(), NULL), /* optional */
                         efl_event_callback_array_add(efl_self, output_buffer_cbs(), NULL) /* optional */
                         );
        if (!output)
          {
             fprintf(stderr, "ERROR: could not open memory buffer.\n");
             retval = EXIT_FAILURE;
             goto end_input;
          }
     }
   else if (strcmp(output_fname, ":none:") == 0)
     {
        /*
         * No output means that the Efl_Io_Copier will cache
         * internally up to efl_io_copier_buffer_limit_set().
         *
         * When finished you can steal copier's buffer in order to use
         * that yourself. See _copier_done().
         */
        output = NULL;
     }
   else if (strncmp(output_fname, "tcp://", strlen("tcp://")) == 0)
     {
        /*
         * Since Efl.Net.Socket implements the required interfaces,
         * they can be used here as well.
         */
        const char *address = output_fname + strlen("tcp://");
        Eina_Error err;
        output = efl_add(EFL_NET_DIALER_TCP_CLASS, NULL,
                         efl_event_callback_array_add(efl_self, output_cbs(), NULL), /* optional */
                         efl_event_callback_array_add(efl_self, dialer_cbs(), NULL) /* optional */
                         );
        if (!output)
          {
             fprintf(stderr, "ERROR: could not create TCP Dialer.\n");
             retval = EXIT_FAILURE;
             goto end_input;
          }

        err = efl_net_dialer_dial(output, address);
        if (err)
          {
             fprintf(stderr, "ERROR: could not TCP dial %s: %s\n",
                     address, eina_error_msg_get(err));
             goto end_output;
          }
     }
   else
     {
        /* regular file, open with flags: write-only, close-on-exec,
         * create if did not exist and truncate if exist.
         */
        output = efl_add(EFL_IO_FILE_CLASS, NULL,
                         efl_file_set(efl_self, output_fname, NULL), /* mandatory */
                         efl_io_file_flags_set(efl_self, O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC), /* mandatory for write */
                         efl_io_file_mode_set(efl_self, 0644), /* mandatory for write */
                         efl_event_callback_array_add(efl_self, output_cbs(), NULL) /* optional */
                         );
        if (!output)
          {
             fprintf(stderr, "ERROR: could not open '%s' for write.\n",
                     output_fname);
             retval = EXIT_FAILURE;
             goto end_input;
          }
     }

   /* A delimiter is optional, if empty or unset, copier will execute
    * writes based on read_chunk_size and only event "data" is emitted.
    *
    * If a line delimiter is set, copier will hold writes until the
    * delimiter is found, source reached End-of-Stream (eos) or the
    * copier buffer limit is reached. The "line" event is emitted.
    */
   if (line_delimiter)
     line_delm_slice = (Eina_Slice)EINA_SLICE_STR(line_delimiter);

   copier = efl_add(EFL_IO_COPIER_CLASS, ecore_main_loop_get(),
                    efl_io_copier_source_set(efl_self, input), /* mandatory */
                    efl_io_copier_destination_set(efl_self, output), /* optional, see :none: */
                    efl_io_copier_line_delimiter_set(efl_self, &line_delm_slice), /* optional */
                    efl_io_copier_buffer_limit_set(efl_self, buffer_limit), /* optional, defaults to unlimited */
                    efl_io_copier_read_chunk_size_set(efl_self, read_chunk_size), /* optional, defaults to 4096 */
                    efl_event_callback_array_add(efl_self, copier_cbs(), NULL) /* recommended, at least EFL_IO_COPIER_EVENT_DONE. */
                    );
   if (!copier)
     {
        fprintf(stderr, "ERROR: could not create copier.\n");
        retval = EXIT_FAILURE;
        goto end_output;
     }

   fprintf(stderr, "INFO: copy source=%p (%s) to destination=%p (%s)\n",
           input,
           efl_class_name_get(efl_class_get(input)),
           output,
           output ? efl_class_name_get(efl_class_get(output)) : ":none:");

   ecore_main_loop_begin();
   efl_io_closer_close(copier);
   efl_del(copier);
   copier = NULL;

 end_output:
   if (output)
     {
        efl_unref(output);
        output = NULL;
     }
 end_input:
   efl_unref(input);
   input = NULL;

 end:
   ecore_con_shutdown();
   ecore_shutdown();

   return retval;
}
