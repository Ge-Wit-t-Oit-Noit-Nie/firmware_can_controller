#ifndef __GWTONN_INCLUDE_SD_CARD_H__
#define __GWTONN_INCLUDE_SD_CARD_H__

#include "fatfs.h"

#ifdef __cplusplus
extern "C"
{
#endif // End of extern "C" block

    /**
     * \brief  Check if the SD Card is present.
     *
     * This function will check if the SD Card is present. It will mount the SD
     * Card and check if it is ready. If it is, it will return FR_OK, else it
     * will return any of the #FRESULT.
     * \retval FR_OK in case of sucess; else any of the #FRESULT.
     */
    FRESULT sd_card_is_present(void);

    /**
     * \brief  Check on the SD Card if a file exists.
     *
     * This function will check if the file exists on the SD Card. It will mount
     * the SD Card and check if the file exists. If it does, it will return FR_OK,
     * else it will return any of the #FRESULT.
     * \param[in] filename the filename to check to
     * \retval FR_OK in case of sucess; else any of the #FRESULT.
     */
    FRESULT file_exists(const char *filename);
    /**
     * \brief  Write a file to the SD Card.
     *
     * Writing to the SD Card requires some extra steps, such as mounting the SD
     * Card and checking the status. This function handles that all. After the
     * checkes, it will open the file for APPEND and write the content of content
     * to the file.
     * \param[in] filename the filename to write to
     * \param[in] content the content to write (must the \0 terminated)
     * \retval FR_OK in case of sucess; else any of the #FRESULT.
     */
    FRESULT
    write_file(const char *filename, const char *content);


#ifdef __cplusplus
}
#endif // End of extern "C" block

#endif //__GWTONN_INCLUDE_SD_CARD_H__