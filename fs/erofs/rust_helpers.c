#include "rust_helpers.h"

static struct kmem_cache *erofs_inode_cachep __read_mostly;

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

int erofs_init_rust(void)
{
	erofs_inode_cachep = kmem_cache_create("erofs_inode",
					       sizeof(struct erofs_inode), 0,
					       SLAB_RECLAIM_ACCOUNT, NULL);
	if (!erofs_inode_cachep)
		return -ENOMEM;
	return 0;
}

void erofs_destroy_rust(void)
{
	if (erofs_inode_cachep)
		kmem_cache_destroy(erofs_inode_cachep);
}

void erofs_init_inode_rust(struct inode *inode)
{
	EROFS_I(inode) = kmem_cache_alloc(erofs_inode_cachep, GFP_KERNEL);
}

void erofs_free_inode_rust(struct inode *inode)
{
	struct erofs_inode *vi = EROFS_I(inode);
	if (vi)
		kmem_cache_free(erofs_inode_cachep, vi);
}

struct inode *erofs_iget_locked_rust_helper(struct super_block *sb, erofs_nid_t nid)
{
	struct inode *inode;
	int err;

	inode = iget5_locked(sb, erofs_squash_ino(nid), erofs_iget5_eq,
			     erofs_iget5_set, &nid);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	err = erofs_fill_inode(inode);
	if(err)
		goto err_out;

	err = erofs_fill_inode_rust(inode, nid);
	if(err)
		goto err_out;

	return inode;
err_out:
	if (err)
		iget_failed(inode);
	return ERR_PTR(err);
}
