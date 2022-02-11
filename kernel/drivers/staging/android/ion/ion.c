// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/staging/android/ion/ion.c
 *
 * Copyright (C) 2011 Google, Inc.
 */

#include <linux/anon_inodes.h>
#include <linux/debugfs.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/dma-buf.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/file.h>
#include <linux/freezer.h>
#include <linux/fs.h>
#include <linux/idr.h>
#include <linux/kthread.h>
#include <linux/list.h>
#include <linux/memblock.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <linux/rbtree.h>
#include <linux/sched/task.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>

#define CREATE_TRACE_POINTS
#include "ion_trace.h"
#include "ion.h"

static struct ion_device *internal_dev;
static struct device *ion_dev;

static int heap_id;
static atomic_long_t total_heap_bytes;

/* this function should only be called while dev->lock is held */
static void ion_buffer_add(struct ion_device *dev,
			   struct ion_buffer *buffer)
{
	struct rb_node **p = &dev->buffers.rb_node;
	struct rb_node *parent = NULL;
	struct ion_buffer *entry;

	while (*p) {
		parent = *p;
		entry = rb_entry(parent, struct ion_buffer, node);

		if (buffer < entry) {
			p = &(*p)->rb_left;
		} else if (buffer > entry) {
			p = &(*p)->rb_right;
		} else {
			pr_err("%s: buffer already found.", __func__);
			BUG();
		}
	}

	rb_link_node(&buffer->node, parent, p);
	rb_insert_color(&buffer->node, &dev->buffers);
}

static void track_buffer_created(struct ion_buffer *buffer)
{
	long total = atomic_long_add_return(buffer->size, &total_heap_bytes);

	trace_ion_stat(buffer->sg_table, buffer->size, total);
}

static void track_buffer_destroyed(struct ion_buffer *buffer)
{
	long total = atomic_long_sub_return(buffer->size, &total_heap_bytes);

	trace_ion_stat(buffer->sg_table, -buffer->size, total);
}

/* this function should only be called while dev->lock is held */
static struct ion_buffer *ion_buffer_create(struct ion_heap *heap,
					    struct ion_device *dev,
					    unsigned long len,
					    unsigned long flags)
{
	struct ion_buffer *buffer;
	int ret;

	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return ERR_PTR(-ENOMEM);

	buffer->heap = heap;
	buffer->flags = flags;
	buffer->dev = dev;
	buffer->size = len;

	ret = heap->ops->allocate(heap, buffer, len, flags);

	if (ret) {
		if (!(heap->flags & ION_HEAP_FLAG_DEFER_FREE))
			goto err2;

		ion_heap_freelist_drain(heap, 0);
		ret = heap->ops->allocate(heap, buffer, len, flags);
		if (ret)
			goto err2;
	}

	if (!buffer->sg_table) {
		WARN_ONCE(1, "This heap needs to set the sgtable");
		ret = -EINVAL;
		goto err1;
	}

	INIT_LIST_HEAD(&buffer->attachments);
	mutex_init(&buffer->lock);

	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct scatterlist *sg;
		struct sg_table *table = buffer->sg_table;
		int i;

		/*
		 * this will set up dma addresses for the sglist -- it is not
		 * technically correct as per the dma api -- a specific
		 * device isn't really taking ownership here.  However, in
		 * practice on our systems the only dma_address space is
		 * physical addresses.
		 */
		for_each_sg(table->sgl, sg, table->nents, i) {
			sg_dma_address(sg) = sg_phys(sg);
			sg_dma_len(sg) = sg->length;
		}
	}

	mutex_lock(&dev->buffer_lock);
	ion_buffer_add(dev, buffer);
	mutex_unlock(&dev->buffer_lock);
	track_buffer_created(buffer);
	return buffer;

err1:
	heap->ops->free(buffer);
err2:
	kfree(buffer);
	return ERR_PTR(ret);
}

void ion_buffer_destroy(struct ion_buffer *buffer)
{
	if (buffer->kmap_cnt > 0) {
		pr_warn_once("%s: buffer still mapped in the kernel\n",
			     __func__);
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
	}
	buffer->heap->ops->free(buffer);
	kfree(buffer);
}

static void _ion_buffer_destroy(struct ion_buffer *buffer)
{
	struct ion_heap *heap = buffer->heap;
	struct ion_device *dev = buffer->dev;

	mutex_lock(&dev->buffer_lock);
	rb_erase(&buffer->node, &dev->buffers);
	mutex_unlock(&dev->buffer_lock);
	track_buffer_destroyed(buffer);

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_freelist_add(heap, buffer);
	else
		ion_buffer_destroy(buffer);
}

static void *ion_buffer_kmap_get(struct ion_buffer *buffer)
{
	void *vaddr;

	if (buffer->kmap_cnt) {
		buffer->kmap_cnt++;
		return buffer->vaddr;
	}
	vaddr = buffer->heap->ops->map_kernel(buffer->heap, buffer);
	if (WARN_ONCE(!vaddr,
		      "heap->ops->map_kernel should return ERR_PTR on error"))
		return ERR_PTR(-EINVAL);
	if (IS_ERR(vaddr))
		return vaddr;
	buffer->vaddr = vaddr;
	buffer->kmap_cnt++;
	return vaddr;
}

static void ion_buffer_kmap_put(struct ion_buffer *buffer)
{
	buffer->kmap_cnt--;
	if (!buffer->kmap_cnt) {
		buffer->heap->ops->unmap_kernel(buffer->heap, buffer);
		buffer->vaddr = NULL;
	}
}

static struct sg_table *dup_sg_table(struct sg_table *table)
{
	struct sg_table *new_table;
	int ret, i;
	struct scatterlist *sg, *new_sg;

	new_table = kzalloc(sizeof(*new_table), GFP_KERNEL);
	if (!new_table)
		return ERR_PTR(-ENOMEM);

	ret = sg_alloc_table(new_table, table->nents, GFP_KERNEL);
	if (ret) {
		kfree(new_table);
		return ERR_PTR(-ENOMEM);
	}

	new_sg = new_table->sgl;
	for_each_sg(table->sgl, sg, table->nents, i) {
		memcpy(new_sg, sg, sizeof(*sg));
		sg_dma_address(new_sg) = 0;
		sg_dma_len(new_sg) = 0;
		new_sg = sg_next(new_sg);
	}

	return new_table;
}

static void free_duped_table(struct sg_table *table)
{
	sg_free_table(table);
	kfree(table);
}

struct ion_dma_buf_attachment {
	struct device *dev;
	struct sg_table *table;
	struct list_head list;
	bool mapped:1;
};

static int ion_dma_buf_attach(struct dma_buf *dmabuf,
			      struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a;
	struct sg_table *table;
	struct ion_buffer *buffer = dmabuf->priv;

	a = kzalloc(sizeof(*a), GFP_KERNEL);
	if (!a)
		return -ENOMEM;

	table = dup_sg_table(buffer->sg_table);
	if (IS_ERR(table)) {
		kfree(a);
		return -ENOMEM;
	}

	a->table = table;
	a->dev = attachment->dev;
	INIT_LIST_HEAD(&a->list);
	a->mapped = false;

	attachment->priv = a;

	mutex_lock(&buffer->lock);
	list_add(&a->list, &buffer->attachments);
	mutex_unlock(&buffer->lock);

	return 0;
}

static void ion_dma_buf_detatch(struct dma_buf *dmabuf,
				struct dma_buf_attachment *attachment)
{
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct ion_buffer *buffer = dmabuf->priv;

	mutex_lock(&buffer->lock);
	list_del(&a->list);
	mutex_unlock(&buffer->lock);
	free_duped_table(a->table);

	kfree(a);
}

static struct sg_table *ion_map_dma_buf(struct dma_buf_attachment *attachment,
					enum dma_data_direction direction)
{
	struct sg_table *table;
	unsigned long map_attrs;
	int count;
	struct ion_dma_buf_attachment *a = attachment->priv;
	struct ion_buffer *buffer = attachment->dmabuf->priv;

	table = a->table;

	map_attrs = attachment->dma_map_attrs;
	if (!(buffer->flags & ION_FLAG_CACHED))
		map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->lock);
	count = dma_map_sg_attrs(attachment->dev, table->sgl,
				 table->nents, direction,
				 map_attrs);
	if (count <= 0) {
		mutex_unlock(&buffer->lock);
		return ERR_PTR(-ENOMEM);
	}

	a->mapped = true;
	mutex_unlock(&buffer->lock);
	return table;
}

static void ion_unmap_dma_buf(struct dma_buf_attachment *attachment,
			      struct sg_table *table,
			      enum dma_data_direction direction)
{
	unsigned long map_attrs;
	struct ion_buffer *buffer = attachment->dmabuf->priv;
	struct ion_dma_buf_attachment *a = attachment->priv;

	map_attrs = attachment->dma_map_attrs;
	if (!(buffer->flags & ION_FLAG_CACHED))
		map_attrs |= DMA_ATTR_SKIP_CPU_SYNC;

	mutex_lock(&buffer->lock);
	dma_unmap_sg_attrs(attachment->dev, table->sgl, table->nents,
			   direction, map_attrs);
	a->mapped = false;
	mutex_unlock(&buffer->lock);
}

static int ion_mmap(struct dma_buf *dmabuf, struct vm_area_struct *vma)
{
	struct ion_buffer *buffer = dmabuf->priv;
	int ret = 0;

	if (!buffer->heap->ops->map_user) {
		pr_err("%s: this heap does not define a method for mapping to userspace\n",
		       __func__);
		return -EINVAL;
	}

	if (!(buffer->flags & ION_FLAG_CACHED))
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	mutex_lock(&buffer->lock);
	/* now map it to userspace */
	ret = buffer->heap->ops->map_user(buffer->heap, buffer, vma);
	mutex_unlock(&buffer->lock);

	if (ret)
		pr_err("%s: failure mapping buffer to userspace\n",
		       __func__);

	return ret;
}

static void ion_dma_buf_release(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;

	_ion_buffer_destroy(buffer);
	kfree(dmabuf->exp_name);
}

static void *ion_dma_buf_vmap(struct dma_buf *dmabuf)
{
	struct ion_buffer *buffer = dmabuf->priv;
	void *vaddr = ERR_PTR(-EINVAL);

	if (buffer->heap->ops->map_kernel) {
		mutex_lock(&buffer->lock);
		vaddr = ion_buffer_kmap_get(buffer);
		mutex_unlock(&buffer->lock);
	} else {
		pr_warn_ratelimited("heap %s doesn't support map_kernel\n",
				    buffer->heap->name);
	}

	return vaddr;
}

static void ion_dma_buf_vunmap(struct dma_buf *dmabuf, void *vaddr)
{
	struct ion_buffer *buffer = dmabuf->priv;

	if (buffer->heap->ops->map_kernel) {
		mutex_lock(&buffer->lock);
		ion_buffer_kmap_put(buffer);
		mutex_unlock(&buffer->lock);
	}
}

static void *ion_dma_buf_kmap(struct dma_buf *dmabuf, unsigned long offset)
{
	/*
	 * TODO: Once clients remove their hacks where they assume kmap(ed)
	 * addresses are virtually contiguous implement this properly
	 */
	void *vaddr = ion_dma_buf_vmap(dmabuf);

	if (IS_ERR(vaddr))
		return vaddr;

	return vaddr + offset * PAGE_SIZE;
}

static void ion_dma_buf_kunmap(struct dma_buf *dmabuf, unsigned long offset,
			       void *ptr)
{
	/*
	 * TODO: Once clients remove their hacks where they assume kmap(ed)
	 * addresses are virtually contiguous implement this properly
	 */
	ion_dma_buf_vunmap(dmabuf, ptr);
}

static int ion_sgl_sync_range(struct device *dev, struct scatterlist *sgl,
			      unsigned int nents, unsigned int offset,
			      unsigned int length,
			      enum dma_data_direction dir, bool for_cpu)
{
	int i;
	struct scatterlist *sg;
	unsigned int len = 0;
	dma_addr_t sg_dma_addr;

	for_each_sg(sgl, sg, nents, i) {
		unsigned int sg_offset, sg_left, size = 0;

		sg_dma_addr = sg_dma_address(sg);

		len += sg->length;
		if (len <= offset)
			continue;

		sg_left = len - offset;
		sg_offset = sg->length - sg_left;

		size = (length < sg_left) ? length : sg_left;
		if (for_cpu)
			dma_sync_single_range_for_cpu(dev, sg_dma_addr,
						      sg_offset, size, dir);
		else
			dma_sync_single_range_for_device(dev, sg_dma_addr,
							 sg_offset, size, dir);

		offset += size;
		length -= size;

		if (length == 0)
			break;
	}

	return 0;
}

static int ion_dma_buf_begin_cpu_access(struct dma_buf *dmabuf,
					enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;

	if (direction == DMA_TO_DEVICE)
		return 0;

	mutex_lock(&buffer->lock);
	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct device *dev = ion_dev;
		struct sg_table *table = buffer->sg_table;

		if (dev) {
			if (buffer->heap->type == ION_HEAP_TYPE_DMA)
				dma_sync_single_range_for_cpu(dev,
							      sg_dma_address(table->sgl),
							      0, buffer->size,
							      direction);
			else
				dma_sync_sg_for_cpu(dev, table->sgl, table->nents,
						    direction);
			goto unlock;
		}
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sg_for_cpu(a->dev, a->table->sgl, a->table->nents,
				    direction);
	}
unlock:
	mutex_unlock(&buffer->lock);

	return 0;
}

static int ion_dma_buf_end_cpu_access(struct dma_buf *dmabuf,
				      enum dma_data_direction direction)
{
	struct ion_buffer *buffer = dmabuf->priv;
	struct ion_dma_buf_attachment *a;

	if (buffer->size >= SZ_1M) {
		if (direction == DMA_FROM_DEVICE) {
			flush_cache_all();
			goto exit;
		} else {
#ifdef CONFIG_ARM64
			__flush_dcache_all();
			goto exit;
#endif
		}
	}

	mutex_lock(&buffer->lock);
	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct device *dev = ion_dev;
		struct sg_table *table = buffer->sg_table;

		if (dev) {
			if (buffer->heap->type == ION_HEAP_TYPE_DMA)
				dma_sync_single_range_for_device(dev,
								 sg_dma_address(table->sgl),
								 0, buffer->size,
								 direction);
			else

				dma_sync_sg_for_device(dev, table->sgl, table->nents,
						       direction);
			goto unlock;
		}
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;
		dma_sync_sg_for_device(a->dev, a->table->sgl, a->table->nents,
				       direction);
	}
unlock:
	mutex_unlock(&buffer->lock);
exit:
	return 0;
}

static int ion_dma_buf_begin_cpu_access_partial(struct dma_buf *dmabuf,
						enum dma_data_direction direction,
						unsigned int offset,
						unsigned int len)
{
	struct device *dev = ion_dev;
	struct ion_buffer *buffer = dmabuf->priv;
	struct sg_table *table = buffer->sg_table;
	struct ion_dma_buf_attachment *a;
	int ret = 0;

	if (direction == DMA_TO_DEVICE)
		return 0;

	mutex_lock(&buffer->lock);
	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		if (dev) {
			if (buffer->heap->type == ION_HEAP_TYPE_DMA)
				dma_sync_single_range_for_cpu(dev,
							      sg_dma_address(table->sgl),
							      offset, len,
							      direction);
			else
				ret = ion_sgl_sync_range(dev, table->sgl, table->nents,
							 offset, len, direction, true);
			goto unlock;
		}
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;

		ret = ion_sgl_sync_range(a->dev, a->table->sgl, a->table->nents,
					 offset, len, direction, true);
	}
unlock:
	mutex_unlock(&buffer->lock);

	return ret;
}

static int ion_dma_buf_end_cpu_access_partial(struct dma_buf *dmabuf,
					      enum dma_data_direction direction,
					      unsigned int offset,
					      unsigned int len)
{
	struct device *dev = ion_dev;
	struct ion_buffer *buffer = dmabuf->priv;
	struct sg_table *table = buffer->sg_table;
	struct ion_dma_buf_attachment *a;
	int ret = 0;

	if (len >= SZ_1M) {
		if (direction == DMA_FROM_DEVICE) {
			flush_cache_all();
			goto exit;
		} else {
#ifdef CONFIG_ARM64
			__flush_dcache_all();
			goto exit;
#endif
		}
	}

	mutex_lock(&buffer->lock);
	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		if (dev) {
			if (buffer->heap->type == ION_HEAP_TYPE_DMA)
				dma_sync_single_range_for_device(dev,
								 sg_dma_address(table->sgl),
								 offset, len,
								 direction);
			else
				ret = ion_sgl_sync_range(dev, table->sgl, table->nents,
							 offset, len, direction, false);
			goto unlock;
		}
	}

	list_for_each_entry(a, &buffer->attachments, list) {
		if (!a->mapped)
			continue;

		ret = ion_sgl_sync_range(a->dev, a->table->sgl, a->table->nents,
					 offset, len, direction, false);
	}
unlock:
	mutex_unlock(&buffer->lock);
exit:
	return ret;
}

static const struct dma_buf_ops dma_buf_ops = {
	.map_dma_buf = ion_map_dma_buf,
	.unmap_dma_buf = ion_unmap_dma_buf,
	.mmap = ion_mmap,
	.release = ion_dma_buf_release,
	.attach = ion_dma_buf_attach,
	.detach = ion_dma_buf_detatch,
	.begin_cpu_access = ion_dma_buf_begin_cpu_access,
	.end_cpu_access = ion_dma_buf_end_cpu_access,
	.begin_cpu_access_partial = ion_dma_buf_begin_cpu_access_partial,
	.end_cpu_access_partial = ion_dma_buf_end_cpu_access_partial,
	.map = ion_dma_buf_kmap,
	.unmap = ion_dma_buf_kunmap,
	.vmap = ion_dma_buf_vmap,
	.vunmap = ion_dma_buf_vunmap,
};

int ion_alloc(size_t len, unsigned int heap_id_mask, unsigned int flags)
{
	struct ion_device *dev = internal_dev;
	struct ion_buffer *buffer = NULL;
	struct ion_heap *heap;
	DEFINE_DMA_BUF_EXPORT_INFO(exp_info);
	int fd;
	struct dma_buf *dmabuf;
	char task_comm[TASK_COMM_LEN];

	pr_debug("%s: len %zu heap_id_mask %u flags %x\n", __func__,
		 len, heap_id_mask, flags);
	/*
	 * traverse the list of heaps available in this system in priority
	 * order.  If the heap type is supported by the client, and matches the
	 * request of the caller allocate from it.  Repeat until allocate has
	 * succeeded or all heaps have been tried
	 */
	len = PAGE_ALIGN(len);

	if (!len)
		return -EINVAL;

	down_read(&dev->lock);
	plist_for_each_entry(heap, &dev->heaps, node) {
		/* if the caller didn't specify this heap id */
		if (!((1 << heap->id) & heap_id_mask))
			continue;
		buffer = ion_buffer_create(heap, dev, len, flags);
		if (!IS_ERR(buffer))
			break;
	}
	up_read(&dev->lock);

	if (!buffer)
		return -ENODEV;

	if (IS_ERR(buffer))
		return PTR_ERR(buffer);

	if (IS_ENABLED(CONFIG_ION_FORCE_DMA_SYNC)) {
		struct device *dev = ion_dev;
		struct sg_table *table = buffer->sg_table;

		if (dev)
			dma_sync_sg_for_device(dev, table->sgl, table->nents,
					       DMA_BIDIRECTIONAL);
	}

	get_task_comm(task_comm, current->group_leader);

	exp_info.ops = &dma_buf_ops;
	exp_info.size = buffer->size;
	exp_info.flags = O_RDWR;
	exp_info.priv = buffer;
	exp_info.exp_name = kasprintf(GFP_KERNEL, "%s-%s-%d-%s", KBUILD_MODNAME,
				      heap->name, current->tgid, task_comm);

	dmabuf = dma_buf_export(&exp_info);
	if (IS_ERR(dmabuf)) {
		_ion_buffer_destroy(buffer);
		kfree(exp_info.exp_name);
		return PTR_ERR(dmabuf);
	}

	fd = dma_buf_fd(dmabuf, O_CLOEXEC);
	if (fd < 0)
		dma_buf_put(dmabuf);

	return fd;
}

int ion_query_heaps(struct ion_heap_query *query)
{
	struct ion_device *dev = internal_dev;
	struct ion_heap_data __user *buffer = u64_to_user_ptr(query->heaps);
	int ret = -EINVAL, cnt = 0, max_cnt;
	struct ion_heap *heap;
	struct ion_heap_data hdata;

	memset(&hdata, 0, sizeof(hdata));

	down_read(&dev->lock);
	if (!buffer) {
		query->cnt = dev->heap_cnt;
		ret = 0;
		goto out;
	}

	if (query->cnt <= 0)
		goto out;

	max_cnt = query->cnt;

	plist_for_each_entry(heap, &dev->heaps, node) {
		strncpy(hdata.name, heap->name, MAX_HEAP_NAME);
		hdata.name[sizeof(hdata.name) - 1] = '\0';
		hdata.type = heap->type;
		hdata.heap_id = heap->id;

		if (copy_to_user(&buffer[cnt], &hdata, sizeof(hdata))) {
			ret = -EFAULT;
			goto out;
		}

		cnt++;
		if (cnt >= max_cnt)
			break;
	}

	query->cnt = cnt;
	ret = 0;
out:
	up_read(&dev->lock);
	return ret;
}

int ion_get_phys(struct ion_phys_data *phys)
{
	struct dma_buf *dmabuf;
	struct ion_buffer *buffer;

	if (IS_ERR_OR_NULL(phys))
		return -EINVAL;

	dmabuf = dma_buf_get(phys->fd);
	if (IS_ERR_OR_NULL(dmabuf))
		return -ENOENT;

	phys->paddr = (__u64)-1;
	buffer = dmabuf->priv;
	if (!IS_ERR_OR_NULL(buffer) &&
	    (buffer->heap->type == ION_HEAP_TYPE_SYSTEM_CONTIG ||
	     buffer->heap->type == ION_HEAP_TYPE_DMA ||
	     buffer->heap->type == ION_HEAP_TYPE_CARVEOUT))
		phys->paddr = sg_phys(buffer->sg_table->sgl);

	dma_buf_put(dmabuf);

	return 0;
}

static const struct file_operations ion_fops = {
	.owner          = THIS_MODULE,
	.unlocked_ioctl = ion_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= ion_ioctl,
#endif
};

static int ion_debug_heap_show(struct seq_file *s, void *unused)
{
	struct ion_heap *heap = s->private;

	if (heap->debug_show)
		heap->debug_show(heap, s, unused);

	return 0;
}

static int ion_debug_heap_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_debug_heap_show, inode->i_private);
}

static const struct file_operations debug_heap_fops = {
	.open = ion_debug_heap_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int debug_shrink_set(void *data, u64 val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = GFP_HIGHUSER;
	sc.nr_to_scan = val;

	if (!val) {
		objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
		sc.nr_to_scan = objs;
	}

	heap->shrinker.scan_objects(&heap->shrinker, &sc);
	return 0;
}

static int debug_shrink_get(void *data, u64 *val)
{
	struct ion_heap *heap = data;
	struct shrink_control sc;
	int objs;

	sc.gfp_mask = GFP_HIGHUSER;
	sc.nr_to_scan = 0;

	objs = heap->shrinker.count_objects(&heap->shrinker, &sc);
	*val = objs;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_shrink_fops, debug_shrink_get,
			debug_shrink_set, "%llu\n");

void ion_device_add_heap(struct ion_heap *heap)
{
	struct ion_device *dev = internal_dev;
	int ret;

	if (!heap->ops->allocate || !heap->ops->free)
		pr_err("%s: can not add heap with invalid ops struct.\n",
		       __func__);

	spin_lock_init(&heap->free_lock);
	heap->free_list_size = 0;

	if (heap->flags & ION_HEAP_FLAG_DEFER_FREE)
		ion_heap_init_deferred_free(heap);

	if ((heap->flags & ION_HEAP_FLAG_DEFER_FREE) || heap->ops->shrink) {
		ret = ion_heap_init_shrinker(heap);
		if (ret)
			pr_err("%s: Failed to register shrinker\n", __func__);
	}

	heap->dev = dev;
	down_write(&dev->lock);
	heap->id = heap_id++;
	/*
	 * use negative heap->id to reverse the priority -- when traversing
	 * the list later attempt higher id numbers first
	 */
	plist_node_init(&heap->node, -heap->id);
	plist_add(&heap->node, &dev->heaps);

	if (heap->shrinker.count_objects && heap->shrinker.scan_objects) {
		char debug_name[64];

		snprintf(debug_name, 64, "%s_shrink", heap->name);
		debugfs_create_file(debug_name, 0644, dev->debug_root,
				    heap, &debug_shrink_fops);
	}

	if (heap->debug_show) {
		char debug_name[64];

		snprintf(debug_name, 64, "%s_stats", heap->name);
		debugfs_create_file(debug_name, 0644, dev->debug_root,
				    heap, &debug_heap_fops);
	}

	dev->heap_cnt++;
	up_write(&dev->lock);

	pr_info("%s: %s id=%d type=%d\n", __func__, heap->name, heap->id, heap->type);
}
EXPORT_SYMBOL(ion_device_add_heap);

static ssize_t
total_heaps_kb_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	u64 size_in_bytes = atomic_long_read(&total_heap_bytes);

	return sprintf(buf, "%llu\n", div_u64(size_in_bytes, 1024));
}

static ssize_t
total_pools_kb_show(struct kobject *kobj, struct kobj_attribute *attr,
		    char *buf)
{
	u64 size_in_bytes = ion_page_pool_nr_pages() * PAGE_SIZE;

	return sprintf(buf, "%llu\n", div_u64(size_in_bytes, 1024));
}

static struct kobj_attribute total_heaps_kb_attr =
	__ATTR_RO(total_heaps_kb);

static struct kobj_attribute total_pools_kb_attr =
	__ATTR_RO(total_pools_kb);

static struct attribute *ion_device_attrs[] = {
	&total_heaps_kb_attr.attr,
	&total_pools_kb_attr.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ion_device);

static int ion_init_sysfs(void)
{
	struct kobject *ion_kobj;
	int ret;

	ion_kobj = kobject_create_and_add("ion", kernel_kobj);
	if (!ion_kobj)
		return -ENOMEM;

	ret = sysfs_create_groups(ion_kobj, ion_device_groups);
	if (ret) {
		kobject_put(ion_kobj);
		return ret;
	}

	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int ion_heaps_show(struct seq_file *s, void *unused)
{
	struct ion_device *dev = internal_dev;
	struct ion_heap *heap;

	down_read(&dev->lock);
	seq_printf(s, "%s\t%s\t%s\n", "id", "type", "name");
	plist_for_each_entry(heap, &dev->heaps, node) {
		seq_printf(s, "%u\t%u\t%s\n", heap->id, heap->type, heap->name);
	}
	up_read(&dev->lock);
	return 0;
}

static int ion_heaps_open(struct inode *inode, struct file *file)
{
	return single_open(file, ion_heaps_show, NULL);
}

static const struct file_operations ion_heaps_operations = {
	.open           = ion_heaps_open,
	.read           = seq_read,
	.llseek         = seq_lseek,
	.release        = single_release,
};
#endif

static const struct platform_device_info ion_dev_info = {
	.name		= "ion",
	.id		= PLATFORM_DEVID_AUTO,
	.dma_mask	= DMA_BIT_MASK(32),
};

static void ion_device_register(void)
{
	struct platform_device *pdev;
	int ret;

	pdev = platform_device_register_full(&ion_dev_info);
	if (pdev) {
		ret = of_dma_configure(&pdev->dev, NULL, true);
		if (ret) {
			platform_device_unregister(pdev);
			pdev = NULL;
		}
	}

	ion_dev = pdev ? &pdev->dev : NULL;
}

static int ion_device_create(void)
{
	struct ion_device *idev;
	int ret;

	idev = kzalloc(sizeof(*idev), GFP_KERNEL);
	if (!idev)
		return -ENOMEM;

	idev->dev.minor = MISC_DYNAMIC_MINOR;
	idev->dev.name = "ion";
	idev->dev.fops = &ion_fops;
	idev->dev.parent = NULL;
	ret = misc_register(&idev->dev);
	if (ret) {
		pr_err("ion: failed to register misc device.\n");
		goto err_reg;
	}

	ret = ion_init_sysfs();
	if (ret) {
		pr_err("ion: failed to add sysfs attributes.\n");
		goto err_sysfs;
	}

	idev->debug_root = debugfs_create_dir("ion", NULL);
#ifdef CONFIG_DEBUG_FS
	debugfs_create_file("heaps", 0444, idev->debug_root, NULL,
			    &ion_heaps_operations);
#endif
	idev->buffers = RB_ROOT;
	mutex_init(&idev->buffer_lock);
	init_rwsem(&idev->lock);
	plist_head_init(&idev->heaps);
	internal_dev = idev;
	ion_device_register();

	return 0;

err_sysfs:
	misc_deregister(&idev->dev);
err_reg:
	kfree(idev);
	return ret;
}

#ifdef CONFIG_ION_MODULE
int ion_module_init(void)
{
	int ret;

	ret = ion_device_create();
#ifdef CONFIG_ION_SYSTEM_HEAP
	if (ret)
		return ret;

	ret = ion_system_heap_create();
	if (ret)
		return ret;

	ret = ion_system_contig_heap_create();
#endif
#ifdef CONFIG_ION_CMA_HEAP
	if (ret)
		return ret;

	ret = ion_add_cma_heaps();
#endif
#ifdef CONFIG_ION_PROTECTED_HEAP
	if (ret)
		return ret;

	ret = ion_protected_heap_create();
#endif
	return ret;
}

module_init(ion_module_init);
#else
subsys_initcall(ion_device_create);
#endif

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Ion memory allocator");
