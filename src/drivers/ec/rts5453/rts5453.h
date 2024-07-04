/* Copyright 2024 Google LLC.  */

/**
 * @brief Board-provided function used by RTS5453 driver to obtain the FW
 *        binary and hash file to flash to the PDC. Board logic is used to
 *        determine the desired image to support multiple variations. A
 *        default implementation of this function returns NULL for both
 *        paths, which causes no update to occur.
 *
 * @param image_path Output pointer to a FW binary path string (or NULL)
 * @param hash_path Output pointer to a FW hash path string (or NULL)
 */
void board_rts5453_get_image_paths(const char **image_path, const char **hash_path);
