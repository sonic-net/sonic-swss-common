/// Take a blocking closure and run it to completion in a [`tokio::task::spawn_blocking`] thread.
pub(crate) async fn spawn_blocking_scoped<F: FnOnce() -> T + Send, T: Send + 'static>(f: F) -> T {
    let clos: Box<dyn FnOnce() -> T + Send> = Box::new(f);
    // SAFETY: We are joining the spawned task before the current lifetime ends, so it is sound to
    // access any data borrowed from this function in the spawned task. However, I acknowledge that
    // extending lifetimes with transmute is terrible. Hail Mary.
    let clos_static: Box<dyn FnOnce() -> T + Send + 'static> = unsafe { std::mem::transmute(clos) };
    ::tokio::task::spawn_blocking(clos_static).await.unwrap()
}

/// These are "basic" because they just use spawn_blocking instead of a more specialized implementation.
/// See [`tokio::task::spawn_blocking`].
macro_rules! impl_basic_async_method {
    // Method (with self)
    ($async_fn:ident <= $sync_fn:ident $(<$($generic_decls:tt),*>)? (& $(mut)? self $(, $arg:ident : $typ:ty)*) $(-> $ret_ty:ty)? $(where $($generic_bounds:tt)*)?) => {
        #[doc = concat!("Async version of [`", stringify!($sync_fn), "`](Self::", stringify!($sync_fn), ") that uses `tokio::task::spawn_blocking`.")]
        pub async fn $async_fn $(<$($generic_decls),*>)? (&mut self $(, $arg: $typ)*) $(-> $ret_ty)? $(where $($generic_bounds)*)? {
            $crate::types::async_util::spawn_blocking_scoped(move || self.$sync_fn($($arg),*)).await
        }
    };

    // Associated fn (without self)
    ($async_fn:ident <= $sync_fn:ident $(<$($generic_decls:tt),*>)? ($($arg:ident : $typ:ty),*) $(-> $ret_ty:ty)? $(where $($generic_bounds:tt)*)?) => {
        #[doc = concat!("Async version of [`", stringify!($sync_fn), "`](Self::", stringify!($sync_fn), ") that uses `tokio::task::spawn_blocking`.")]
        pub async fn $async_fn $(<$($generic_decls),*>)? ($($arg: $typ),*) $(-> $ret_ty)? $(where $($generic_bounds)*)? {
            $crate::types::async_util::spawn_blocking_scoped(move || Self::$sync_fn($($arg),*)).await
        }
    };
}
pub(crate) use impl_basic_async_method;

macro_rules! impl_read_data_async {
    () => {
        /// Async version of [`read_data`](Self::read_data). Does not time out or interrupt on signal.
        pub async fn read_data_async(&mut self) -> ::std::io::Result<()> {
            use ::tokio::io::{unix::AsyncFd, Interest};

            let fd = self.get_fd().map_err(::std::io::Error::other)?;
            let _ready_guard = AsyncFd::with_interest(fd, Interest::READABLE)?.readable().await?;
            self.read_data(Duration::from_secs(0), false)
                .map_err(::std::io::Error::other)?;
            Ok(())
        }
    };
}
pub(crate) use impl_read_data_async;
