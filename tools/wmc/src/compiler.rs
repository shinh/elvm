pub trait Backend<Target> {
    type Target;
    type Error;

    fn compile(&self) -> Result<Self::Target, Self::Error>;
}
