#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <limits.h>
#include <inttypes.h>

/* Reuse Android official U-Boot definitions */
#define BOOT_CTRL_MAGIC		0x42414342 /* Bootloader Control AB */
#define BOOT_CTRL_VERSION	1
#define BOOT_SLOT_NAME(slot)	('a' + (slot))

/* Android official misc partition offset rules */
static const size_t BOOTLOADER_MESSAGE_OFFSET_IN_MISC;
static const size_t WIPE_PACKAGE_OFFSET_IN_MISC = 16 * 1024;

/* Official bootloader_message structure (2048 bytes) */
struct bootloader_message {
	char command[32];
	char status[32];
	char recovery[768];
	char stage[32];
	char reserved[1184];
} __attribute__((packed));

/* Verify structure size (matches official) */
_Static_assert(sizeof(struct bootloader_message) == 2048,
	       "struct bootloader_message must be 2048 bytes");

/* Official bootloader_message_ab structure (4096 bytes) */
struct bootloader_message_ab {
	struct bootloader_message message;
	char slot_suffix[32]; /* bootloader_control storage location */
	char update_channel[128];
	char reserved[1888];
} __attribute__((packed));

/* Verify structure size (matches official) */
_Static_assert(sizeof(struct bootloader_message_ab) == 4096,
	       "struct bootloader_message_ab must be 4096 bytes");

/* Official slot_metadata bitfield definition */
struct slot_metadata {
	uint8_t priority : 4;	      /* Priority 0-15 */
	uint8_t tries_remaining : 3;  /* Remaining retry attempts 0-7 */
	uint8_t successful_boot : 1;  /* Boot successful flag */
	uint8_t verity_corrupted : 1; /* Verity corruption flag */
	uint8_t reserved : 7;	      /* Reserved bits */
} __attribute__((packed));

/* Official bootloader_control structure (32 bytes) */
struct bootloader_control {
	char slot_suffix[4];
	uint32_t magic;
	uint8_t version;
	uint8_t nb_slot : 3;
	uint8_t recovery_tries_remaining : 3;
	uint8_t reserved0[2];
	struct slot_metadata slot_info[4];
	uint8_t reserved1[8];
	uint32_t crc32_le;
} __attribute__((packed));

/* Verify: bootloader_control must fit in slot_suffix */
_Static_assert(sizeof(struct bootloader_control) ==
	       sizeof(((struct bootloader_message_ab *)0)->slot_suffix),
	       "struct bootloader_control must fit in slot_suffix");

/* U-Boot style CRC32 calculation (little-endian, matches official) */
static uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
	static const uint32_t crc_table[256] = {
		0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
		0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
		0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
		0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
		0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
		0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
		0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
		0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
		0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
		0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
		0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
		0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
		0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
		0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
		0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
		0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
		0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
		0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
		0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
		0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
		0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
		0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
		0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
		0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
		0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
		0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
		0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
		0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
		0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
		0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
		0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
		0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
		0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
		0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
		0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
		0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
		0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
		0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
		0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
		0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
		0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
		0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
		0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
		0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
		0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
		0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
		0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
		0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
		0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
		0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
		0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
		0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
		0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
		0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
		0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
		0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
		0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
		0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
		0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
		0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
		0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
		0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
		0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
		0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
	};

	crc ^= 0xffffffff;
	while (len--)
		crc = crc_table[(crc ^ *buf++) & 0xff] ^ (crc >> 8);

	return crc ^ 0xffffffff;
}

/* Calculate CRC32 for bootloader_control (official rule: first 28 bytes) */
static uint32_t ab_control_compute_crc(const struct bootloader_control *abc)
{
	return crc32(0, (const uint8_t *)abc,
		     offsetof(struct bootloader_control, crc32_le));
}

/* Get block device size */
static int get_block_size(const char *dev_path, uint32_t *blksz)
{
	int fd;

	fd = open(dev_path, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n",
			dev_path, strerror(errno));
		return -1;
	}

	if (ioctl(fd, BLKBSZGET, blksz) != 0) {
		if (ioctl(fd, BLKSSZGET, blksz) != 0) {
			fprintf(stderr, "Failed to get block size: %s\n",
				strerror(errno));
			close(fd);
			return -1;
		}
	}

	close(fd);
	printf("Device %s block size: %u bytes\n", dev_path, *blksz);
	return 0;
}

/* Official offset calculation: bootloader_control location in misc (2048 bytes) */
static ulong get_abc_byte_offset(void)
{
	ulong offset = sizeof(struct bootloader_message); /* 2048 bytes */

	printf("Official ABC offset in misc: %lu bytes (0x%lx)\n",
	       offset, offset);
	return offset;
}

/* Read bootloader_control (follows official U-Boot logic) */
static int ab_control_read(const char *misc_dev,
			  struct bootloader_control **abc)
{
	uint32_t blksz;
	ulong abc_byte_offset = get_abc_byte_offset();
	ulong abc_block_offset;
	ulong abc_blocks;
	int fd, ret;
	void *buf = NULL;

	/* 1. Get block size */
	if (get_block_size(misc_dev, &blksz) != 0)
		return -1;

	/* 2. U-Boot core: convert to block offset */
	if (abc_byte_offset % blksz != 0) {
		fprintf(stderr,
			"Error: ABC offset (0x%lx) not aligned to block size (%u)\n",
			abc_byte_offset, blksz);
		return -1;
	}

	abc_block_offset = abc_byte_offset / blksz;
	abc_blocks = (sizeof(struct bootloader_control) + blksz - 1) / blksz;

	/* 3. Allocate block-aligned buffer */
	buf = aligned_alloc(blksz, abc_blocks * blksz);
	if (!buf) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}
	memset(buf, 0, abc_blocks * blksz);

	/* 4. Open device and seek to block offset */
	fd = open(misc_dev, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n",
			misc_dev, strerror(errno));
		free(buf);
		return -1;
	}

	if (lseek(fd, abc_byte_offset, SEEK_SET) < 0) {
		fprintf(stderr, "Failed to lseek to 0x%lx: %s\n",
			abc_byte_offset, strerror(errno));
		close(fd);
		free(buf);
		return -1;
	}

	/* 5. Read block by block */
	ret = read(fd, buf, abc_blocks * blksz);
	if (ret != (int)(abc_blocks * blksz)) {
		fprintf(stderr, "Read failed: got %d bytes, expected %lu\n",
			ret, abc_blocks * blksz);
		close(fd);
		free(buf);
		return -1;
	}
	close(fd);

	/* 6. Extract bootloader_control (first 32 bytes) */
	*abc = malloc(sizeof(struct bootloader_control));
	if (!*abc) {
		free(buf);
		return -1;
	}
	memcpy(*abc, buf, sizeof(struct bootloader_control));

	free(buf);
	printf("Successfully read ABC from %s (block offset: %lu)\n",
	       misc_dev, abc_block_offset);
	return 0;
}

/* Write bootloader_control (follows official U-Boot logic) */
static int ab_control_write(const char *misc_dev,
			    struct bootloader_control *abc)
{
	uint32_t blksz;
	ulong abc_byte_offset = get_abc_byte_offset();
	ulong abc_block_offset;
	ulong abc_blocks;
	int fd, ret;
	void *buf = NULL;

	/* 1. Recalculate CRC */
	abc->crc32_le = ab_control_compute_crc(abc);

	/* 2. Get block size */
	if (get_block_size(misc_dev, &blksz) != 0)
		return -1;

	/* 3. Calculate block offset */
	abc_block_offset = abc_byte_offset / blksz;
	abc_blocks = (sizeof(struct bootloader_control) + blksz - 1) / blksz;

	/* 4. Allocate buffer and copy data */
	buf = aligned_alloc(blksz, abc_blocks * blksz);
	if (!buf) {
		fprintf(stderr, "Out of memory\n");
		return -1;
	}
	memset(buf, 0, abc_blocks * blksz);
	memcpy(buf, abc, sizeof(struct bootloader_control));

	/* 5. Write to device */
	fd = open(misc_dev, O_WRONLY);
	if (fd < 0) {
		fprintf(stderr, "Failed to open %s for write: %s\n",
			misc_dev, strerror(errno));
		free(buf);
		return -1;
	}

	if (lseek(fd, abc_byte_offset, SEEK_SET) < 0) {
		fprintf(stderr, "Failed to lseek to 0x%lx: %s\n",
			abc_byte_offset, strerror(errno));
		close(fd);
		free(buf);
		return -1;
	}

	ret = write(fd, buf, abc_blocks * blksz);
	if (ret != (int)(abc_blocks * blksz)) {
		fprintf(stderr, "Write failed: wrote %d bytes, expected %lu\n",
			ret, abc_blocks * blksz);
		close(fd);
		free(buf);
		return -1;
	}

	fsync(fd);
	close(fd);
	free(buf);

	printf("Successfully wrote ABC to %s (block offset: %lu)\n",
	       misc_dev, abc_block_offset);
	return 0;
}

/* Initialize default ABC (U-Boot official defaults) */
static void ab_control_default(struct bootloader_control *abc)
{
	int i;

	memset(abc, 0, sizeof(struct bootloader_control));

	/* Official default values */
	memcpy(abc->slot_suffix, "a\0\0\0", 4);
	abc->magic = BOOT_CTRL_MAGIC;
	abc->version = BOOT_CTRL_VERSION;
	abc->nb_slot = 2; /* NUM_SLOTS */

	/* Default slot metadata */
	for (i = 0; i < 2; i++) {
		abc->slot_info[i].priority = 15;
		abc->slot_info[i].tries_remaining = 3;
		abc->slot_info[i].successful_boot = 0;
		abc->slot_info[i].verity_corrupted = 0;
		abc->slot_info[i].reserved = 0;
	}

	abc->crc32_le = ab_control_compute_crc(abc);
}

/* Print ABC info (parse bitfields) */
static void ab_control_print(struct bootloader_control *abc)
{
	int i;

	if (!abc)
		return;

	printf("\n=== A/B Control Block (U-Boot Official) ===\n");
	printf("Magic: 0x%08x (expected: 0x%08x) %s\n",
	       abc->magic, BOOT_CTRL_MAGIC,
	       abc->magic == BOOT_CTRL_MAGIC ? "[OK]" : "[ERROR]");
	printf("Version: %d (supported: %d)\n",
	       abc->version, BOOT_CTRL_VERSION);
	printf("Number of slots: %d\n", abc->nb_slot);
	printf("Current active slot: %s\n", abc->slot_suffix);
	printf("Recovery tries remaining: %d\n",
	       abc->recovery_tries_remaining);
	printf("CRC32: 0x%08x (computed: 0x%08x) %s\n",
	       abc->crc32_le, ab_control_compute_crc(abc),
	       abc->crc32_le == ab_control_compute_crc(abc) ? "[OK]" : "[ERROR]");

	/* Print metadata for each slot */
	for (i = 0; i < abc->nb_slot; i++) {
		printf("\nSlot %c:\n", BOOT_SLOT_NAME(i));
		printf("  Priority: %d\n", abc->slot_info[i].priority);
		printf("  Tries remaining: %d\n",
		       abc->slot_info[i].tries_remaining);
		printf("  Successful boot: %d %s\n",
		       abc->slot_info[i].successful_boot,
		       abc->slot_info[i].successful_boot ? "[YES]" : "[NO]");
		printf("  Verity corrupted: %d\n",
		       abc->slot_info[i].verity_corrupted);
	}
	printf("=====================================================\n\n");
}

/* Core function: mark current active slot as successfully booted */
static int mark_current_slot_successful(const char *misc_dev)
{
	struct bootloader_control *abc = NULL;
	char current_slot;
	int slot_idx, ret;

	/* 1. Read current ABC */
	if (ab_control_read(misc_dev, &abc) != 0) {
		fprintf(stderr, "Failed to read ABC from %s\n", misc_dev);
		return -1;
	}

	/* 2. Validate magic and CRC */
	if (abc->magic != BOOT_CTRL_MAGIC ||
	    abc->crc32_le != ab_control_compute_crc(abc)) {
		fprintf(stderr,
			"Warning: Invalid ABC (bad magic/CRC), resetting to default...\n");
		ab_control_default(abc);
	}

	/* 3. Get current active slot */
	if (abc->slot_suffix[0] == '\0' || abc->slot_suffix[0] < 'a' ||
	    abc->slot_suffix[0] >= 'a' + abc->nb_slot) {
		fprintf(stderr, "Error: Invalid current slot suffix: %s\n",
			abc->slot_suffix);
		free(abc);
		return -1;
	}

	current_slot = abc->slot_suffix[0];
	slot_idx = current_slot - 'a';

	printf("=== Marking current active slot '%c' as successful ===\n",
	       current_slot);

	/* 4. Mark as successfully booted */
	abc->slot_info[slot_idx].successful_boot = 1;
	abc->slot_info[slot_idx].verity_corrupted = 0;
	abc->slot_info[slot_idx].tries_remaining = 3;
	abc->slot_info[slot_idx].priority = 15;
	abc->crc32_le = ab_control_compute_crc(abc);

	/* 5. Print updated info */
	ab_control_print(abc);

	/* 6. Write to device */
	ret = ab_control_write(misc_dev, abc);

	/* 7. Cleanup */
	free(abc);

	if (ret == 0)
		printf("Success: Slot '%c' marked as successfully booted!\n",
		       current_slot);
	else
		fprintf(stderr, "Failed to mark slot '%c' as successful\n",
			current_slot);

	return ret;
}

/* Compatibility function: manually mark specified slot as successful */
static int mark_specific_slot_successful(const char *misc_dev, char slot)
{
	struct bootloader_control *abc = NULL;
	int slot_idx = slot - 'a';
	int ret;

	/* 1. Read current ABC */
	if (ab_control_read(misc_dev, &abc) != 0) {
		fprintf(stderr, "Failed to read ABC from %s\n", misc_dev);
		return -1;
	}

	/* 2. Validate magic and CRC */
	if (abc->magic != BOOT_CTRL_MAGIC ||
	    abc->crc32_le != ab_control_compute_crc(abc)) {
		fprintf(stderr,
			"Warning: Invalid ABC (bad magic/CRC), resetting to default...\n");
		ab_control_default(abc);
	}

	/* 3. Validate slot */
	if (slot_idx < 0 || slot_idx >= abc->nb_slot) {
		fprintf(stderr,
			"Error: Invalid slot '%c' (valid slots: 0-%d = a-%c)\n",
			slot, abc->nb_slot - 1, 'a' + abc->nb_slot - 1);
		free(abc);
		return -1;
	}

	printf("=== Marking specified slot '%c' as successful ===\n", slot);

	/* 4. Mark as successfully booted */
	abc->slot_info[slot_idx].successful_boot = 1;
	abc->slot_info[slot_idx].tries_remaining = 3;
	abc->slot_info[slot_idx].priority = 15;
	memcpy(abc->slot_suffix, (char[]){slot, '\0', '\0', '\0'}, 4);
	abc->crc32_le = ab_control_compute_crc(abc);

	/* 5. Print updated info */
	ab_control_print(abc);

	/* 6. Write to device */
	ret = ab_control_write(misc_dev, abc);

	/* 7. Cleanup */
	free(abc);

	if (ret == 0)
		printf("Success: Slot '%c' marked as successfully booted!\n", slot);
	else
		fprintf(stderr, "Failed to mark slot '%c' as successful\n", slot);

	return ret;
}

/* Basic function: switch to specified slot (used after OTA upgrade) */
static int switch_to_slot(const char *misc_dev, char target_slot)
{
	struct bootloader_control *abc = NULL;
	int target_idx = target_slot - 'a';
	char original_slot;
	int ret;

	/* 1. Read current ABC */
	if (ab_control_read(misc_dev, &abc) != 0) {
		fprintf(stderr, "Failed to read ABC from %s\n", misc_dev);
		return -1;
	}

	/* 2. Validate magic and CRC */
	if (abc->magic != BOOT_CTRL_MAGIC ||
	    abc->crc32_le != ab_control_compute_crc(abc)) {
		fprintf(stderr,
			"Warning: Invalid ABC (bad magic/CRC), resetting to default...\n");
		ab_control_default(abc);
	}

	/* 3. Validate target slot */
	if (target_idx < 0 || target_idx >= abc->nb_slot) {
		fprintf(stderr,
			"Error: Invalid target slot '%c' (valid slots: 0-%d = a-%c)\n",
			target_slot, abc->nb_slot - 1,
			'a' + abc->nb_slot - 1);
		free(abc);
		return -1;
	}

	/* 4. Record original slot and switch */
	original_slot = abc->slot_suffix[0];
	printf("=== Switching active slot from '%c' to '%c' (OTA upgrade) ===\n",
	       original_slot, target_slot);

	/* Core: update active slot suffix */
	memcpy(abc->slot_suffix, (char[]){target_slot, '\0', '\0', '\0'}, 4);

	/* Reset target slot retry count (follows OTA spec) */
	abc->slot_info[target_idx].tries_remaining = 3;
	abc->slot_info[target_idx].successful_boot = 0;
	abc->slot_info[target_idx].verity_corrupted = 0;
	abc->slot_info[target_idx].priority = 15;

	/* Recalculate CRC */
	abc->crc32_le = ab_control_compute_crc(abc);

	/* 5. Print updated info */
	ab_control_print(abc);

	/* 6. Write to device */
	ret = ab_control_write(misc_dev, abc);

	/* 7. Cleanup */
	free(abc);

	if (ret == 0)
		printf("Success: Active slot switched to '%c'! Next boot will use this slot.\n",
		       target_slot);
	else
		fprintf(stderr, "Failed to switch to slot '%c'\n",
			target_slot);

	return ret;
}

/* New core function: auto-switch to the other slot (no target needed) */
static int switch_to_other_slot(const char *misc_dev)
{
	struct bootloader_control *abc = NULL;
	char current_slot, target_slot;

	/* 1. Read current ABC */
	if (ab_control_read(misc_dev, &abc) != 0) {
		fprintf(stderr, "Failed to read ABC from %s\n", misc_dev);
		return -1;
	}

	/* 2. Validate magic and CRC */
	if (abc->magic != BOOT_CTRL_MAGIC ||
	    abc->crc32_le != ab_control_compute_crc(abc)) {
		fprintf(stderr,
			"Warning: Invalid ABC (bad magic/CRC), resetting to default...\n");
		ab_control_default(abc);
	}

	/* 3. Get current active slot */
	current_slot = abc->slot_suffix[0];
	if (current_slot == '\0' || current_slot < 'a' ||
	    current_slot >= 'a' + abc->nb_slot) {
		fprintf(stderr, "Error: Invalid current slot suffix: %s\n",
			abc->slot_suffix);
		free(abc);
		return -1;
	}

	/* 4. Auto-calculate target slot (supports 2-slot systems) */
	if (abc->nb_slot != 2) {
		fprintf(stderr,
			"Error: Auto-switch only supports 2 slots (current: %d slots)\n",
			abc->nb_slot);
		free(abc);
		return -1;
	}

	target_slot = (current_slot == 'a') ? 'b' : 'a';
	printf("=== Auto-switch: Current slot is '%c', switching to '%c' ===\n",
	       current_slot, target_slot);

	/* 5. Use base switch function */
	return switch_to_slot(misc_dev, target_slot);
}

/* Help message */
static void show_help(const char *progname)
{
	printf("Usage: %s [OPTIONS]\n", progname);
	printf("A/B Control Block Tool (U-Boot Official)\n\n");
	printf("Options:\n");
	printf("  -d, --device <dev>    Path to misc partition (e.g. /dev/mmcblk0p4) [REQUIRED]\n");
	printf("  -s, --show            Show current A/B control block info\n");
	printf("  -m, --mark-success    Automatically mark CURRENT active slot as successfully booted\n");
	printf("  -M, --mark <slot>     Manually mark SPECIFIED slot (a/b) as successfully booted\n");
	printf("  -r, --reset           Reset A/B control block to U-Boot default values\n");
	printf("  -S, --switch <slot>   Switch active slot to SPECIFIED slot (a/b) [OTA upgrade]\n");
	printf("  -A, --auto-switch     Auto-switch to the OTHER slot (detect current slot automatically)\n");
	printf("  -h, --help            Show this help message\n");
	printf("\nExamples:\n");
	printf("  # Show current ABC info\n");
	printf("  %s -d /dev/mmcblk0p4 -s\n", progname);
	printf("  # Auto mark current active slot as successful (most used)\n");
	printf("  %s -d /dev/mmcblk0p4 -m\n", progname);
	printf("  # Manually mark slot b as successful\n");
	printf("  %s -d /dev/mmcblk0p4 -M b\n", progname);
	printf("  # Reset ABC to default\n");
	printf("  %s -d /dev/mmcblk0p4 -r\n", progname);
	printf("  # Switch active slot to b (after OTA upgrade)\n");
	printf("  %s -d /dev/mmcblk0p4 -S b\n", progname);
	printf("  # Auto-switch to the other slot (no need to specify target)\n");
	printf("  %s -d /dev/mmcblk0p4 -A\n", progname);
}

int main(int argc, char *argv[])
{
	const char *misc_dev = NULL;
	int show_info = 0, mark_current = 0, reset_abc = 0, auto_switch = 0;
	char mark_specific = 0, switch_target = 0;
	int opt;

	/* Check root privileges */
	if (getuid() != 0) {
		fprintf(stderr, "Error: This tool requires root privileges\n");
		return 1;
	}

	/* Parse command line arguments */
	static struct option long_options[] = {
		{"device",	required_argument,	0,	'd'},
		{"show",	no_argument,		0,	's'},
		{"mark-success",	no_argument,		0,	'm'},
		{"mark",	required_argument,	0,	'M'},
		{"reset",	no_argument,		0,	'r'},
		{"switch",	required_argument,	0,	'S'},
		{"auto-switch",	no_argument,		0,	'A'},
		{"help",	no_argument,		0,	'h'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "d:smM:rS:Ah",
				   long_options, NULL)) != -1) {
		switch (opt) {
		case 'd':
			misc_dev = optarg;
			break;
		case 's':
			show_info = 1;
			break;
		case 'm':
			mark_current = 1;
			break;
		case 'M':
			if (optarg[0] == 'a' || optarg[0] == 'b')
				mark_specific = optarg[0];
			else {
				fprintf(stderr,
					"Error: Invalid slot '%s' (must be a/b)\n",
					optarg);
				show_help(argv[0]);
				return 1;
			}
			break;
		case 'r':
			reset_abc = 1;
			break;
		case 'S':
			if (optarg[0] == 'a' || optarg[0] == 'b')
				switch_target = optarg[0];
			else {
				fprintf(stderr,
					"Error: Invalid slot '%s' (must be a/b)\n",
					optarg);
				show_help(argv[0]);
				return 1;
			}
			break;
		case 'A':
			auto_switch = 1;
			break;
		case 'h':
			show_help(argv[0]);
			return 0;
		default:
			show_help(argv[0]);
			return 1;
		}
	}

	/* Check required parameter */
	if (!misc_dev) {
		fprintf(stderr,
			"Error: --device is required (specify misc partition)\n");
		show_help(argv[0]);
		return 1;
	}

	/* Execute operations */
	if (show_info) {
		struct bootloader_control *abc = NULL;

		if (ab_control_read(misc_dev, &abc) == 0) {
			ab_control_print(abc);
			free(abc);
		} else {
			return 1;
		}
	}

	if (mark_current) {
		if (mark_current_slot_successful(misc_dev) != 0)
			return 1;
	}

	if (mark_specific) {
		if (mark_specific_slot_successful(misc_dev, mark_specific) != 0)
			return 1;
	}

	if (switch_target) {
		if (switch_to_slot(misc_dev, switch_target) != 0)
			return 1;
	}

	if (auto_switch) {
		if (switch_to_other_slot(misc_dev) != 0)
			return 1;
	}

	if (reset_abc) {
		struct bootloader_control *abc;

		abc = malloc(sizeof(struct bootloader_control));
		if (!abc) {
			fprintf(stderr, "Out of memory\n");
			return 1;
		}
		ab_control_default(abc);
		printf("=== Resetting ABC to U-Boot default values ===\n");
		ab_control_print(abc);
		if (ab_control_write(misc_dev, abc) == 0)
			printf("Success: ABC reset to default!\n");
		else {
			fprintf(stderr, "Failed to reset ABC\n");
			free(abc);
			return 1;
		}
		free(abc);
	}

	/* Check if at least one operation was specified */
	if (!show_info && !mark_current && !mark_specific &&
	    !switch_target && !auto_switch && !reset_abc) {
		fprintf(stderr,
			"Error: No operation specified (use -s, -m, -M, -S, -A, or -r)\n");
		show_help(argv[0]);
		return 1;
	}

	return 0;
}