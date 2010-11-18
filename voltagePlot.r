# R functions for plotting voltages over time.
#
# Required libraries: r-cran-zoo

library(zoo)

read.voltages <- function(file) {
  a <- read.csv(file)
  a$datetime <- as.POSIXct(a$microseconds/1e6, origin="1970-01-01")
  a
}

voltageplot <- function(t, main="Akun jännite", avg=20) {
  # Moving average
  na.start <- floor(avg/2)
  na.stop <- ceiling(avg/2)-1

  volts.avg <- c(rep(NA,na.start),rollmean(t$voltage.in,avg),rep(NA,na.stop))

  plot(t$datetime, t$voltage.in, format="%H.%M", pch=1, col="grey",
       main=main,xlab="aika",ylab="jännite, V")
  
  lines(t$datetime, volts.avg)
}
