package dist_app_environment.peopleinyear_spark

import java.io.File
import org.apache.commons.io.FileUtils
import org.apache.spark.launcher.SparkLauncher
import org.apache.spark.rdd.RDD
import org.apache.spark.sql.SparkSession
import org.apache.spark.sql.functions.col

object PeopleInYear {
  def main(args: Array[String]) {
    if (args.length < 2) {
      System.err.println("Usage: peopleinyear <hdfs_in> [<hdfs_in>...] <hdfs_out>\n"
        + "hdfs_* args should start with hdfs:// for an HDFS filesystem or file:// for a local filesystem")
      System.exit(2)
    }
    // environment variable of Spark master node defaults to local[*] if the the master URL is absent
    if (System.getProperty(SparkLauncher.SPARK_MASTER) == null) {
      System.setProperty(SparkLauncher.SPARK_MASTER, "local[*]")
    }
    // create a Spark session for the job with respect to the environment variable above
    val spark = SparkSession.builder.appName(PeopleInYear.getClass.getSimpleName).getOrCreate()

    /* load and process as DataFrame */
    // read files (based on "fs.defaultFS" for absent schema, "hdfs://..." if set HADOOP_CONF_DIR env-var, or "file://")
    val inputDf = spark.read.format("csv")
      .option("header", "true") // uses the first line as names of columns
      .option("inferSchema", "true") // infers the input schema automatically from data (one extra pass over the data)
      .load(args.dropRight(1): _*)
    // get sum for each year column
    val countsDf = inputDf.select("JMÉNO", "2016")
      .filter(_.get(0).asInstanceOf[String].startsWith("MA"))
      .groupBy().sum("2016")
    // dump results
    val outputPathDf = args.last + "/as-dataframe"
    FileUtils.deleteDirectory(new File(outputPathDf))
    countsDf.write.format("csv").option("header", "true").save(outputPathDf)

    /* load as DataFrame and process as RDD (utilize `inputDF` and `columnNamesForYears` defined above) */
    // convert the input data in DataFrame to RDD
    val inputRddFromDf = inputDf.rdd
    // filter rows starting with MA and get the 2016 value
    val yearsRddFromDf = inputRddFromDf
      .filter(row => row.getString(0).startsWith("MA"))
      .map(row => row.getInt(117))
    // sum it all
    val countsRddFromDf = yearsRddFromDf.reduce(_ + _)

    println("People in 2016 (dataframe->rdd):")
    println(countsRddFromDf)

    /* load and process as RDD */
    // read files (based on "fs.defaultFS" for absent schema, "hdfs://..." if set HADOOP_CONF_DIR env-var, or "file://")
    val inputRdd = spark.sparkContext.textFile(args.dropRight(1).mkString(","))
    // drop the header (the first line)
    val inputWithoutHeadersRdd = inputRdd.mapPartitionsWithIndex((idx, itr) => if (idx == 0) itr.drop(1) else itr)
    // filter rows starting with MA and get the 2016 value
    val yearsRdd = inputWithoutHeadersRdd
      .filter(row => row.startsWith("MA"))
      .map(row => row.split(',')(117).toInt)
    // sum it all
    val countsRdd = yearsRdd.reduce(_ + _)

    println("People in 2016 (rdd):")
    println(countsRdd)
  }
}
