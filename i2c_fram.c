#include <stdio.h>
#include <i2c_fram/i2c_fram.h>

/* Pointer to the linked list storing the memory structures. */
static i2c_fram_s * framArr = NULL;
/* Number of i2c memories reigstered so far */
static uint8_t num_i2c_fram = 0U;
/* Upper bound of all i2c FRAMs combined */
static uint32_t i2c_fram_max_address = 0U;

/**
 * \brief This function is used by higher level function to read dat aover i2c in blocking mode.
 */
int32_t __attribute__((weak)) i2c_fram_i2c_read_bytes(uint8_t addr, uint8_t * buff, uint32_t len) {
  return 0;
}

/**
 * \brief This function is used by the higher level functions to send data over i2c in blocking mode.
 *        This function must be redefined by the programmer to write data over i2c in blocking mode.
 *        This function must ensure that the i2c bus per txns max limit is not violated.
 */
int32_t __attribute__((weak)) i2c_fram_i2c_send_bytes(uint8_t addr, uint8_t * buff, uint32_t len) {
  return 0;
}

/**
 * \brief This function is used by the higher level functions to send data over i2c in blocking mode.
 *        This function must be redefined by the programmer to write data over i2c in blocking mode.
 *        This function must ensure that the i2c bus per txns max limit is not violated.
 */
int32_t __attribute__((weak)) i2c_fram_i2c_send_bytes_opts(uint8_t addr, uint8_t * buff, uint32_t len, bool keep) {
  return 0;
}

/**
 * \brief Function to validate an i2c FRAM's details.
 *
 * \return 0 if valid, -1 otherwise
 */
static int32_t i2c_fram_validate_fram(i2c_fram_s * fram) {
  int32_t ret = 0;

  do {
    /* Validate the size */
    if (fram->size > I2C_FRAM_MAX_SIZE_BYTES) {
      ret = -1;
      break;
    }
  } while(0U);

  return ret;
}

/**
 * \brief Function to register i2c FRAM memories for usage.
 */
int32_t i2c_fram_register(i2c_fram_s * fram) {
  int32_t ret = 0;
  i2c_fram_s * tmp_ptr;

  /* Ensure that FRAM structure pointer is not null */
  if ((fram == NULL) ||
      (num_i2c_fram == I2C_FRAM_NUM_FRAM_MAX)) {
    return -1;
  }

  /* Validate the new FRAM structure */
  ret = i2c_fram_validate_fram(fram);
  if (ret != 0) {
    return ret;
  }

  /* List is empty? */
  if (framArr == NULL) {
    /* Insert as the first node in the linked list */
    fram->prev = NULL;
    fram->next = NULL;
    framArr = fram;

    /* Update the state variables */
    i2c_fram_max_address += fram->size;
    num_i2c_fram += 1U;
  } else {
    tmp_ptr = framArr;

    /* Iterate till the last node */
    while(tmp_ptr->next != NULL) {
      tmp_ptr = tmp_ptr->next;
    }

    fram->prev = tmp_ptr;
    fram->next = NULL;
    tmp_ptr->next = fram;

    /* Update the state variables */
    i2c_fram_max_address += fram->size;
    num_i2c_fram += 1U;
  }

  return ret;
}

/**
 * \brief Function to initialize all the registered i2c FRAM memories.
 */
int32_t i2c_fram_init(void) {
  int32_t ret = 0;
  /* Pointer to the linked list */
  i2c_fram_s * tmp_ptr = framArr;

  while (1) {
    if (tmp_ptr->wp_gpio_handler != NULL) {
      tmp_ptr->wp_gpio_handler(0U);
    }

    tmp_ptr->initialized = 1U;
    /* last ? */
    if (tmp_ptr->next == NULL) break;
    /* go to next */
    tmp_ptr = tmp_ptr->next;
  }

  return ret;
}

int32_t i2c_fram_read_write_raw(i2c_fram_s * fram, uint8_t op, uint32_t address, uint8_t * data, uint32_t len) {
  int32_t ret = 0;
  bool keep;
  uint8_t fram_addr[2];
  /* MSByte */
  fram_addr[0] = ((address >> 8U) & 0xFFU);
  /* LSByte */
  fram_addr[1] = ((address >> 0U) & 0xFFU);

  if (op == I2C_FRAM_OP_READ) {
    keep = false;
  } else if (op == I2C_FRAM_OP_WRITE) {
    keep = true;
  } else {
    /* Invalid operation type */
  }

  ret = i2c_fram_i2c_send_bytes_opts(fram->i2c_address, &fram_addr[0], sizeof(fram_addr), keep);
  if (ret == 0) {
    if (op == I2C_FRAM_OP_READ) {
      ret = i2c_fram_i2c_read_bytes(fram->i2c_address, data, len);
    } else if (op == I2C_FRAM_OP_WRITE) {
      ret = i2c_fram_i2c_send_bytes_opts(fram->i2c_address, data, len, false);
    }
  }

  return ret;
}

/**
 * \brief Function to write to the i2c FRAM. The function automatically selects
 * the right i2c memory to write to based on the input address.
 */
int32_t i2c_fram_read_write(uint32_t address, uint8_t op, uint8_t * bytes, uint32_t len) {
  int32_t ret = 0;

  uint32_t tmp = 0U;
  uint32_t offset = 0U;
  uint32_t last_boundary = 0U;
  uint32_t start_address = address;

  i2c_fram_s * tmp_ptr = framArr;

  printf("[i2c_fram_read_write] Starting write: address=0x%x, len=%u\n", address, len);

  /* Validate the input */
  if ((bytes == NULL) ||
      ((address + len) >= i2c_fram_max_address)) {
    printf("[i2c_fram_read_write] ERROR: Invalid input (bytes=%p, max_address=%u, requested_end=0x%x)\n",
           bytes, i2c_fram_max_address, address + len);
    return -1;
  }

  while (len) {

    if (start_address == 0U) {
      /* bytes to write in this iteration */
      tmp = (len > tmp_ptr->size)?(tmp_ptr->size):len;
      // READ/WRITE OP
      printf("[i2c_fram_read_write] Write to FRAM[0x%02x]: offset=0x%x, bytes=%u\n",
             tmp_ptr->i2c_address, start_address, tmp);
      ret = i2c_fram_read_write_raw(tmp_ptr, op, start_address, &bytes[offset], tmp);
    } else if ((start_address >= last_boundary) &&
               (start_address < (last_boundary + tmp_ptr->size))) {
      /* FRAM address to write to */
      start_address = (start_address - last_boundary);

      tmp = (tmp_ptr->size - start_address);
      /* bytes to write in this iteration */
      tmp = (len > tmp)?tmp:len;
      // READ/WRITE OP
      printf("[i2c_fram_read_write] Write to FRAM[0x%02x]: offset=0x%x, bytes=%u\n",
             tmp_ptr->i2c_address, start_address, tmp);
      ret = i2c_fram_read_write_raw(tmp_ptr, op, start_address, &bytes[offset], tmp);
      start_address = 0U;
    }
    offset += tmp;
    len -= tmp;

    /* Programming done ? */
    if (len == 0U) {
      printf("[i2c_fram_read_write] Write complete: total_bytes_written=%u\n", offset);
      break;
    }

    /* Last node, exit */
    if (tmp_ptr->next == NULL) {
      printf("[i2c_fram_read_write] WARNING: Reached last FRAM but len=%u bytes remaining\n", len);
      break;
    }
    
    last_boundary += tmp_ptr->size;
    /* Next node */
    tmp_ptr = tmp_ptr->next;
    /* Update the last boundary */
    printf("[i2c_fram_read_write] Moving to next FRAM, remaining_len=%u\n", len);
  }

  return ret;
}

#if defined(I2C_FRAM_ENABLE_TEST_FUNCS)
uint8_t i2c_fram_test_iterate(void) {
  uint8_t count = 0U;
  i2c_fram_s * tmp_ptr;
  tmp_ptr = framArr;

  printf("[i2c_fram_test_iterate] Starting iteration\n");

  if (tmp_ptr == NULL) {
    printf("[i2c_fram_test_iterate] No FRAM registered!\n");
    return count;
  }

  printf("[i2c_fram_test_iterate] Total FRAMs registered: %d\n", num_i2c_fram);

  while (1) {
    printf("[i2c_fram_test_iterate] FRAM[%d]: size=%u bytes, i2c_addr=0x%02x, initialized=%d\n",
           count, tmp_ptr->size, tmp_ptr->i2c_address, tmp_ptr->initialized);
    count++;
    if (tmp_ptr->next == NULL) break;
    tmp_ptr = tmp_ptr->next;
  }

  printf("[i2c_fram_test_iterate] Iteration complete, found %d FRAMs, max_address=%u\n", count, i2c_fram_max_address);

  return count;
}
#endif

