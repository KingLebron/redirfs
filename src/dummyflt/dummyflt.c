#include "../redirfs/redirfs.h"

static rfs_filter dummyflt;
static struct rfs_path_info path_info;

enum rfs_retv dummyflt_permission(rfs_context context, struct rfs_args *args);
enum rfs_retv dummyflt_open(rfs_context context, struct rfs_args *args);

static struct rfs_filter_info flt_info = {"dummyflt", 1000, 0};

static struct rfs_op_info op_info[] = {
	{RFS_REG_IOP_PERMISSION, dummyflt_permission, dummyflt_permission},
	{RFS_DIR_IOP_PERMISSION, dummyflt_permission, dummyflt_permission},
	{RFS_REG_FOP_OPEN, dummyflt_open, dummyflt_open},
	{RFS_DIR_FOP_OPEN, dummyflt_open, dummyflt_open},
	{RFS_OP_END, NULL, NULL}
};

enum rfs_retv dummyflt_permission(rfs_context context, struct rfs_args *args)
{
	printk(KERN_ALERT "dummyflt: permission: dentry: %s, call: %s, file: %s\n",
		args->args.i_permission.nd ? (char *)args->args.i_permission.nd->dentry->d_name.name : "",
		(args->type.call == RFS_PRECALL) ? "precall" : "postcall",
		S_ISDIR(args->args.i_permission.inode->i_mode) ? "dir" : "reg");

	return RFS_CONTINUE;
}

enum rfs_retv dummyflt_open(rfs_context context, struct rfs_args *args)
{
	printk(KERN_ALERT "dummyflt: open: dentry: %s, call: %s, file: %s\n",
		args->args.f_open.file->f_dentry->d_name.name, 
		(args->type.call == RFS_PRECALL) ? "precall" : "postcall",
		S_ISDIR(args->args.f_open.inode->i_mode) ? "dir" : "reg");

	return RFS_CONTINUE;
}

enum rfs_err dummyflt_mod_cb(union rfs_mod *mod)
{
	enum rfs_err err;
	switch (mod->id){
		case RFS_ACTIVATE:
			err = rfs_activate_filter(dummyflt);
			break;
		case RFS_DEACTIVATE:
			err = rfs_deactivate_filter(dummyflt);
			break;
		case RFS_SET_PATH:
			err = rfs_set_path(dummyflt, &mod->set_path.path_info);
			break;
		default:
			err = RFS_ERR_NOENT;
	}
	return err;
}

static int __init dummyflt_init(void)
{
	enum rfs_err err;

	err = rfs_register_filter(&dummyflt, &flt_info);
	if (err != RFS_ERR_OK) {
		printk(KERN_ERR "dummyflt: register filter failed: error %d\n", err);
		goto error;
	}

	err = rfs_set_operations(dummyflt, op_info); 
	if (err != RFS_ERR_OK) {
		printk(KERN_ERR "dummyflt: set operations failed: error %d\n", err);
		goto error;
	}

#error "Please fill the path_info.path variable with the full pathname which you want to use and delete this line!!!"

	path_info.path = "";
	path_info.flags = RFS_PATH_INCLUDE | RFS_PATH_SUBTREE;

	err = rfs_set_path(dummyflt, &path_info); 
	if (err != RFS_ERR_OK) {
		printk(KERN_ERR "dummyflt: set path failed: error %d\n", err);
		goto error;
	}

	err = rfs_activate_filter(dummyflt);
	if (err != RFS_ERR_OK) {
		printk(KERN_ERR "dummyflt: activate filter failed: error %d\n", err);
		goto error;
	}

	err = rfs_set_mod_cb(dummyflt, &dummyflt_mod_cb);
	if (err != RFS_ERR_OK) {
		printk(KERN_ERR "dummyflt: set filter modification callback failed: error %d\n", err);
		goto error;
	}

	return 0;

error:
	if (rfs_unregister_filter(dummyflt))
		printk(KERN_ERR "dummyflt: unregister filter failed: error %d\n", err);

	return err;
}

static void __exit dummyflt_exit(void)
{
	enum rfs_err err;
	
	err = rfs_unregister_filter(dummyflt);
	if (err != RFS_ERR_OK)
		printk(KERN_ERR "dummyflt: unregistration failed: error %d\n", err);
}

module_init(dummyflt_init);
module_exit(dummyflt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Frantisek Hrbata <franta@redirfs.org>");
MODULE_DESCRIPTION("Dummy Filter for the RedirFS Framework");