#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>

#define BUFFER_SIZE 1024
#define BYTES_SIZE 8

/**
 * Function prototypes for the adler driver.
 */
static int adler_open(devminor_t minor, int access, endpoint_t user_endpt);
static int adler_close(devminor_t minor);
static ssize_t adler_read(devminor_t minor, u64_t position, endpoint_t endpt,
                          cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t adler_write(devminor_t minor, u64_t UNUSED(position), endpoint_t endpt,
        cp_grant_id_t grant, size_t size, int UNUSED(flags), cdev_id_t UNUSED(id));

/** SEF functions and variables. */
static int sef_cb_init(int type, sef_init_info_t *info);
static void sef_local_startup(void);
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);

/** Entry points to the adler driver. */
static struct chardriver adler_tab =
        {
                .cdr_open	= adler_open,
                .cdr_close	= adler_close,
                .cdr_read	= adler_read,
                .cdr_write	= adler_write,
        };

// Adler-32 variables
static uint32_t A;
static uint32_t B;
const uint32_t MOD = 65521;
static unsigned char buffer[BUFFER_SIZE];

static int adler_open(devminor_t UNUSED(minor), int UNUSED(access), endpoint_t UNUSED(user_endpt)) {
    return OK;
}

static int adler_close(devminor_t UNUSED(minor)) {
    return OK;
}

static ssize_t adler_read(devminor_t UNUSED(minor), u64_t position, endpoint_t endpt,
                          cp_grant_id_t grant, size_t size, int UNUSED(flags), cdev_id_t UNUSED(id)) {
    u64_t dev_size = BYTES_SIZE;
    unsigned char ptr[BYTES_SIZE + 1];
    int ret;

    if (size < dev_size)
        return EINVAL;
    if (position >= dev_size) /* EOF */
        return 0;
    if (position + size > dev_size)
        size = (size_t)(dev_size - position); /* limit size */

    sprintf(ptr, "%08x", (B << 16) | A);
    /* Restore default */
    A = 1;
    B = 0;

    /* Copy the requested part to the caller. */
    if ((ret = sys_safecopyto(endpt, grant, 0, (vir_bytes) (ptr + (size_t)position), size)) != OK)
        return ret;

    /* Return the number of bytes read. */
    return size;
}

static ssize_t adler_write(devminor_t UNUSED(minor), u64_t UNUSED(position), endpoint_t endpt,
                           cp_grant_id_t grant, size_t size, int UNUSED(flags), cdev_id_t UNUSED(id)) {
    size_t chunk = 0;
    int tmp;

    for (size_t offset = 0; offset < size; offset += chunk) {
        chunk = MIN(size - offset, BUFFER_SIZE);
        tmp = sys_safecopyfrom(endpt, grant, offset, (vir_bytes)buffer, chunk);
        if (tmp != OK) {
            printf("Adler: sys_safecopyfrom failed for proc %d, grant %d\n", endpt, grant);
            return tmp;
        }
        for (size_t i = 0; i < chunk; ++i) {
            A = (A + buffer[i]) % MOD;
            B = (B + A) % MOD;
        }
    }

    return size;
}

/** Initialize the adler driver. */
static int sef_cb_init(int type, sef_init_info_t* UNUSED(info)) {
    int do_announce_driver = TRUE;

    A = 1;
    B = 0;
    switch(type) {
        case SEF_INIT_LU:
            /* Restore the state. */
            lu_state_restore();
            do_announce_driver = FALSE;
            break;

        case SEF_INIT_FRESH:
        case SEF_INIT_RESTART:
            break;
    }

    /* Announce we are up when necessary. */
    if (do_announce_driver) {
        chardriver_announce();
    }

    /* Initialization completed successfully. */
    return OK;
}

static void sef_local_startup() {
    /*
     * Register init callbacks. Use the same function for all event types
     */
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_lu(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);

    /*
     * Register live update callbacks.
     */
    /* - Agree to update immediately when LU is requested in a valid state. */
    sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
    /* - Support live update starting from any standard state. */
    sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
    /* - Register a custom routine to save the state. */
    sef_setcb_lu_state_save(sef_cb_lu_state_save);

    /* Let SEF perform startup. */
    sef_startup();
}

/** Save the state. */
static int sef_cb_lu_state_save(int UNUSED(state)) {
    ds_publish_u32("A", A, DSF_OVERWRITE);
    ds_publish_u32("B", B, DSF_OVERWRITE);
    return OK;
}

/** Restore the state. */
static int lu_state_restore() {
    u32_t value;

    ds_retrieve_u32("A", &value);
    ds_delete_u32("A");
    A = value;

    ds_retrieve_u32("B", &value);
    ds_delete_u32("B");
    B = value;

    return OK;
}

int main(void) {
    sef_local_startup();
    chardriver_task(&adler_tab);
    return OK;
}

