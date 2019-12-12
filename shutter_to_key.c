#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/input.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <signal.h>

static FILE *fp;
static int fd0, debug=0;
static char *version_model, *version_release, *keymask_filename;

static int version_load()
{
	FILE *fp;
	char *line = NULL;
	size_t len = 0, i = 0;
	ssize_t read;

	fp = fopen("/etc/version.info", "r");
	if (fp != NULL) {
		if ((read = getline(&line, &len, fp)) != -1) {
			line[strcspn(line, "\r\n")] = 0;
			asprintf(&version_release, "%s", line);
		}
		if ((read = getline(&line, &len, fp)) != -1) {
			line[strcspn(line, "\r\n")] = 0;
			asprintf(&version_model, "%s", line);
		}
		fclose(fp);
		free(line);
		return 0;
	}
	printf("Unable to determine device model and firmware version!\n");
	exit(1);
	return -1;
}

int unmask_shutter() {
	unsigned char keymask_clean[] = {
	0x30, 0x78, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x0a
	};
	unsigned int keymask_clean_len = 11;
	fp=fopen(keymask_filename, "w");
	if (fp) {
		fwrite(keymask_clean, 1, keymask_clean_len, fp);
		fclose(fp);
	}
}

int mask_shutter() {
	unsigned char keymask_masked[] = {
	0x30, 0x78, 0x30, 0x30, 0x30, 0x30, 0x31, 0x38, 0x30, 0x30, 0x0a
	};
	unsigned int keymask_masked_len = 11;
	fp=fopen(keymask_filename, "w");
	if (fp) {
		fwrite(keymask_masked, 1, keymask_masked_len, fp);
		fclose(fp);
	}
}

void sig_handler(int signo)
{
	printf("Caught INT signal - Exiting ...\n");
	unmask_shutter();
	exit(0);
}

int main (int argc, char *argv[])
{
	char keys_buffer[11];
	char *pushed_keys;
	fd_set inputs;
	int n, s1, s2, old_s1=0, old_s2=0, prev_trans=0;
    char *key1="ok",*key2="rec",*remap1="st key click name_of_the_key_for_s1", *remap2="st key click name_of_the_key_for_s2";

	if (signal(SIGINT, sig_handler) == SIG_ERR)
		printf("ERROR: Unable to catch SIGUSR1!\n"); // Catch INT (CTRL+C) -> unmask and exit
	
   if (argc > 1) {
	   key1=argv[1];
   }

   if (argc > 2) {
	   key2=argv[2];
   }
    
	version_load();
	printf("Running shutter_to_key S1->%s S2->%s on %s v %s\n",key1,key2,version_model, version_release);
    printf("Usage: shutter_to_key [name_of_the_key_for_s1] [name_of_the_key_for_s2]\n");
    asprintf(&remap1,"st key click %s",key1);
    asprintf(&remap2,"st key click %s",key2);
	
	if (strcmp("NX500",version_model)==0){
		asprintf(&pushed_keys,"%s","/sys/devices/platform/d5-keys/key_shutter");
		asprintf(&keymask_filename,"%s","/sys/devices/platform/d5-keys/keymask");
	} else {
		asprintf(&pushed_keys,"%s","/sys/devices/platform/d5-keys/key_shutter");
		asprintf(&keymask_filename,"%s","/sys/devices/platform/d5keys-polled/keymask");
	}
	mask_shutter();

	printf("Opening %s\n",pushed_keys);
    while (1) {
		fd0 = open(pushed_keys, O_RDONLY);
		if (fd0 == -1) {
			fprintf(stderr, "Error opening input file %s: %s.\n", pushed_keys, strerror(errno));
			return EXIT_FAILURE;
		}
		n = read(fd0, &keys_buffer, 2);
		close(fd0);
		s1 = (keys_buffer[0] & 1) > 0;
		s2 = (keys_buffer[0] & 2) > 0;
		s1 += s2;
		if (old_s2 == 1 && s2 == 0) {
			if (debug) printf("S2 S1: %d\tS2: %d\n", s1, s2);
			system(remap2);
			s1=0;
			usleep(500000); // you have 0.5s to release the shutter
		} else if (old_s1 == 1 && s1 == 0) {
			if (debug) printf("S1 S1: %d\tS2: %d\n", s1, s2);
			system(remap1);
		}
		old_s1=s1;
		old_s2=s2;
		usleep(50000); // inotify doesn't work on this file :(
	}
	unmask_shutter();
}
