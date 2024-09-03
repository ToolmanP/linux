#include "rust_helpers.h"

static void erofs_init_metabuf_rust_helper(struct erofs_buf *buf,
					   struct super_block *sb,
					   struct erofs_sb_info *sbi)
{
	if (erofs_is_fileio_mode(sbi))
		buf->mapping = file_inode(sbi->fdev)->i_mapping;
	else if (erofs_is_fscache_mode_rust_helper(sb, sbi))
		buf->mapping = sbi->s_fscache->inode->i_mapping;
	else
		buf->mapping = sb->s_bdev->bd_mapping;
}

void *erofs_read_metabuf_rust_helper(struct super_block *sb,
				     struct erofs_sb_info *sbi,
				     erofs_off_t offset)
{
	struct erofs_buf buf = __EROFS_BUF_INITIALIZER;
	erofs_init_metabuf_rust_helper(&buf, sb, sbi);
	return erofs_bread(&buf, offset, EROFS_KMAP);
}

void erofs_put_metabuf_rust_helper(void *addr)
{
	erofs_put_metabuf(&(struct erofs_buf){
		.base = addr,
		.page = kmap_to_page(addr),
		.kmap_type = EROFS_KMAP,
	});
}
